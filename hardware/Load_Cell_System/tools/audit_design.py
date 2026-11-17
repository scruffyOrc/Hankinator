"""Audit exported native netlists and board files against separately transcribed interface requirements.
Does not import build_design.py or its manifest for expected electrical mappings.
Run with KiCad's bundled Python.
"""
from pathlib import Path
import xml.etree.ElementTree as ET,json,math,collections
import pcbnew as p
ROOT=Path(__file__).resolve().parent.parent;OUT=ROOT/'verification';OUT.mkdir(exist_ok=True)
expected={
 'breakout':{'J1':['HX_SCK','GND','HX_FL','3V3_SENSOR',None,'HX_RL','HX_FR','HX_RR'],
 'J2':['HX_FL','HX_SCK','3V3_SENSOR','GND',None,'HX_RL'],'J3':['HX_FR','HX_SCK','3V3_SENSOR','GND',None,'HX_RR']},
 'rear-smart-foot':{'J1':['FRONT_DOUT','HX_SCK','3V3_SENSOR','GND',None,'REAR_DOUT'],'J2':['FRONT_DOUT','HX_SCK','3V3_SENSOR','GND']},
 'front-smart-foot':{'J1':['FRONT_DOUT','HX_SCK','3V3_SENSOR','GND']}}
# Component-side pad coordinates transcribed from the three manufacturer hole-pattern drawings.
# Origin: manufacturer body center. KiCad +X right, +Y down. Not taken from builder or Eagle.
drawings={
 '615008144221':{'pads':{1:(4.445,4.65),2:(3.175,7.19),3:(1.905,4.65),4:(.635,7.19),5:(-.635,4.65),6:(-1.905,7.19),7:(-3.175,4.65),8:(-4.445,7.19)},'holes':[(-5.715,-1.7,3.17),(5.715,-1.7,3.17)]},
 '615006143621':{'pads':{1:(-3.175,-7.71),2:(-1.905,-5.17),3:(-.635,-7.71),4:(.635,-5.17),5:(1.905,-7.71),6:(3.175,-5.17)},'holes':[(-5.08,1.18,3.25),(5.08,1.18,3.25)]},
 '615004143821':{'pads':{1:(1.53,-1.8),2:(.51,-4.34),3:(-.51,-1.8),4:(-1.53,-4.34)},'holes':[(-5,.5,2.36),(5,.5,2.36)]}}
checks=[];positions=[];netlists={};boards={}
def check(cond,msg):
 checks.append({'pass':bool(cond),'check':msg})
 if not cond:print('FAIL:',msg)
def norm(s):return None if not s or s.startswith('unconnected-') else s.lstrip('/')
def close(a,b):return abs(a-b)<.00001
for role,conns in expected.items():
 d=ROOT/('hankinator-lc-'+role);b=p.LoadBoard(str(d/(d.name+'.kicad_pcb')));boards[role]=b
 nl=ET.parse(d/'checks/netlist.xml');pn={}
 for net in nl.findall('.//nets/net'):
  for node in net.findall('node'):pn[(node.get('ref'),node.get('pin'))]=norm(net.get('name'))
 netlists[role]=pn
 for ref,nets in conns.items():
  for i,n in enumerate(nets,1):check(pn.get((ref,str(i)))==n,f'{role} {ref}.{i} = {n or "NC"}')
 for fp in b.GetFootprints():
  for pad in fp.Pads():
   n=pad.GetNumber()
   if n:check(norm(pad.GetNetname())==pn.get((fp.GetReference(),n)),f'{role} schematic/PCB net parity {fp.GetReference()}.{n}')
  if str(fp.GetFPID().GetLibNickname())=='HankinatorLC':
   mpn=str(fp.GetFPID().GetLibItemName()).removeprefix('WE_');pat=drawings[mpn]
   angle=math.radians(fp.GetOrientationDegrees());cx=p.ToMM(fp.GetPosition().x);cy=p.ToMM(fp.GetPosition().y)
   for pad in fp.Pads():
    n=pad.GetNumber();ax=p.ToMM(pad.GetPosition().x);ay=p.ToMM(pad.GetPosition().y)
    if not n:continue
    xx,yy=pat['pads'][int(n)];ex=cx+xx*math.cos(angle)+yy*math.sin(angle);ey=cy-xx*math.sin(angle)+yy*math.cos(angle)
    check(close(ax,ex) and close(ay,ey),f'{role} {fp.GetReference()} pin {n}: drawing to placed pad transform')
    check(close(p.ToMM(pad.GetDrillSize().x),.9),f'{role} {fp.GetReference()}.{n} 0.90mm PTH')
    positions.append({'board':role,'ref':fp.GetReference(),'mpn':mpn,'pin':int(n),'x_mm':round(ax,4),'y_mm':round(ay,4),'net':norm(pad.GetNetname()) or 'NC'})
   actualholes=[]
   for pad in fp.Pads():
    if pad.GetAttribute()!=p.PAD_ATTRIB_NPTH:continue
    ax=p.ToMM(pad.GetPosition().x)-cx;ay=p.ToMM(pad.GetPosition().y)-cy
    actualholes.append((round(ax*math.cos(angle)-ay*math.sin(angle),4),round(ax*math.sin(angle)+ay*math.cos(angle),4),round(p.ToMM(pad.GetDrillSize().x),4)))
   check(sorted(actualholes)==sorted(pat['holes']),f'{role} {fp.GetReference()} mechanical holes match drawing')
 check(len(list(b.GetTracks()))==0 and b.GetAreaCount()==0,f'{role} no routing or filled copper released')
 dims={'breakout':(50,44),'rear-smart-foot':(72,18),'front-smart-foot':(56,18)}[role]
 box=b.GetBoardEdgesBoundingBox();check(close(p.ToMM(box.GetWidth()),dims[0]+.05) and close(p.ToMM(box.GetHeight()),dims[1]+.05),f'{role} outline dimensions {dims}mm (edge stroke included by API)')
 if role!='breakout':
  hx={1:'3V3_SENSOR',2:None,3:'AVDD',4:'GND',5:'GND',6:None,7:'AIN_N',8:'AIN_P',9:'GND',10:'GND',11:'SCK_LOCAL',12:'DOUT_LOCAL',13:None,14:'GND',15:'GND',16:'3V3_SENSOR'}
  for n,net in hx.items():check(pn.get(('U1',str(n)))==net,f'{role} HX711 pin {n} reference-circuit check')
  for i,net in enumerate(['AVDD','GND','BRIDGE_A_P','BRIDGE_A_N'],1):check(pn.get(('J3',str(i)))==net,f'{role} bridge connector {i}')
  if role=='rear-smart-foot':
   frontnodes={node for node,n in pn.items() if n=='FRONT_DOUT'}
   check(frontnodes=={('J1','1'),('J2','1')},'rear FRONT_DOUT connects only J1.1 and J2.1; no local HX711 branch')

# Build the five-board system from native netlists; each cable maps pin n to n.
parent={}
def find(x):
 parent.setdefault(x,x)
 if parent[x]!=x:parent[x]=find(parent[x])
 return parent[x]
def union(a,b):parent[find(a)]=find(b)
instances={'split':'breakout','RL':'rear-smart-foot','RR':'rear-smart-foot','FL':'front-smart-foot','FR':'front-smart-foot'}
for inst,role in instances.items():
 groups=collections.defaultdict(list)
 for (ref,pin),net in netlists[role].items():
  if net:groups[net].append((inst,ref,pin))
 for nodes in groups.values():
  for node in nodes[1:]:union(nodes[0],node)
 # Signal series resistors conduct in the end-to-end continuity model; do not bridge pull-downs.
 if role!='breakout':
  for ref in ['R4','R5']:union((inst,ref,'1'),(inst,ref,'2'))
for branch,rear,front in [('J2','RL','FL'),('J3','RR','FR')]:
 for n in range(1,7):union(('split',branch,str(n)),(rear,'J1',str(n)))
 for n in range(1,5):union((rear,'J2',str(n)),(front,'J1',str(n)))
for inst,inp in [('FL','3'),('RL','6'),('FR','7'),('RR','8')]:
 check(find((inst,'U1','12'))==find(('split','J1',inp)),f'Harness DOUT {inst}: HX711.12 -> split J1.{inp}')
 for other in instances:
  if other not in ['split',inst]:check(find((inst,'U1','12'))!=find((other,'U1','12')),f'{inst} DOUT isolated from {other}')
 check(find((inst,'U1','11'))==find(('split','J1','1')),f'{inst} common SCK reaches HX711.11 through local R5')
 check(find((inst,'U1','16'))==find(('split','J1','4')),f'{inst} supply reaches DVDD')
 check(find((inst,'U1','5'))==find(('split','J1','2')),f'{inst} ground reaches AGND')
check(all(find(('split','J1','5'))!=find(('split','J1',str(n))) for n in [1,2,3,4,6,7,8]),'8P8C pin 5 reserved isolated')
result={'checks_passed':sum(c['pass'] for c in checks),'checks_failed':sum(not c['pass'] for c in checks),'checks':checks,'limit':'Desk audit by same author using independent source representations; not an independent human signoff or physical plug continuity test.'}
(OUT/'desk-audit.json').write_text(json.dumps(result,indent=2));(OUT/'physical-pad-mapping.json').write_text(json.dumps(positions,indent=2))
lines=['# Electrical and orientation desk audit','',f"Passed: {result['checks_passed']}; failed: {result['checks_failed']}.",'',result['limit'],'','The end-to-end check uses KiCad-exported XML netlists, five board instances, contact-number-preserving cables and R4/R5 continuity. It does not treat the schematic generator manifest as the expected interface.','', 'All source-coordinate checks use the saved board footprint positions, not a diagram overlay.','']
lines+=['- '+('PASS' if c['pass'] else 'FAIL')+': '+c['check'] for c in checks]
(OUT/'desk-audit.md').write_text('\n'.join(lines))
print(result['checks_passed'],'passed;',result['checks_failed'],'failed')
if result['checks_failed']:raise SystemExit(1)
