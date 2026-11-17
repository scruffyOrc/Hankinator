"""Generate the Hank Touchscreen Adapter Rev A KiCad board.

Run with KiCad's Python: "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" generate_adapter.py
"""
from pathlib import Path
import pcbnew

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Hank_Touchscreen_Adapter.kicad_pcb"
LIB = Path(r"C:\Program Files\KiCad\10.0\share\kicad\footprints")
MM = pcbnew.FromMM
V = lambda x, y: pcbnew.VECTOR2I(MM(x), MM(y))
board = pcbnew.BOARD()

# Six layers keep the retrofit within the LCD outline without signal jumpers
# or crossed routes. This is intentionally a compact reliability tradeoff.
layers = pcbnew.LSET.AllCuMask(6)
layers.AddLayerSet(pcbnew.LSET.AllTechMask())
board.SetEnabledLayers(layers)

def net(name):
    n = pcbnew.NETINFO_ITEM(board, name); board.Add(n); return n

N = {n: net(n) for n in (
    "+5V", "+3V3", "GND", "GP1_BL_PWM", "BL_GATE", "TFT_LED",
    "TFT_CS", "TFT_RST", "TFT_DC", "TFT_MOSI", "TFT_SCK",
    "TOUCH_CLK", "TOUCH_CS", "TOUCH_DIN", "TOUCH_DO", "TOUCH_IRQ")}

def fp(lib, name, ref, value, x, y, rot=0):
    f = pcbnew.FootprintLoad(str(LIB / lib), name)
    if f is None: raise RuntimeError(f"Missing footprint {lib}/{name}")
    f.SetReference(ref); f.SetValue(value); f.SetPosition(V(x,y)); f.SetOrientationDegrees(rot)
    f.Reference().SetVisible(False); f.Value().SetVisible(False)
    board.Add(f); return f

def nets(f, mapping):
    for p, n in mapping.items():
        pad = f.FindPadByNumber(str(p))
        if not pad: raise RuntimeError(f"{f.GetReference()} missing pad {p}")
        if n: pad.SetNet(N[n])

def xy(f, p):
    pos=f.FindPadByNumber(str(p)).GetPosition()
    return (pcbnew.ToMM(pos.x), pcbnew.ToMM(pos.y))

def track(n, pts, layer, w=.25):
    for a,b in zip(pts,pts[1:]):
        t=pcbnew.PCB_TRACK(board); t.SetNet(N[n]); t.SetStart(V(*a)); t.SetEnd(V(*b))
        t.SetLayer(layer); t.SetWidth(MM(w)); board.Add(t)

def via(n, x, y, top, bottom):
    v=pcbnew.PCB_VIA(board); v.SetNet(N[n]); v.SetPosition(V(x,y)); v.SetWidth(MM(.70)); v.SetDrill(MM(.35))
    v.SetLayerPair(top,bottom); board.Add(v)

def label(s,x,y,size=.85,thick=.15,angle=0):
    size=max(size,.80)
    t=pcbnew.PCB_TEXT(board); t.SetText(s); t.SetPosition(V(x,y)); t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(V(size,size)); t.SetTextThickness(MM(thick)); t.SetTextAngleDegrees(angle)
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER); t.SetVertJustify(pcbnew.GR_TEXT_V_ALIGN_CENTER)
    board.Add(t)

def edge(a,b):
    s=pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_SEGMENT); s.SetStart(V(*a)); s.SetEnd(V(*b))
    s.SetWidth(MM(.1)); s.SetLayer(pcbnew.Edge_Cuts); board.Add(s)

def zone(layer):
    z=pcbnew.ZONE(board); z.SetNet(N["GND"]); z.SetLayer(layer); z.SetLocalClearance(MM(.25))
    z.SetMinThickness(MM(.20)); z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    poly=z.Outline(); poly.NewOutline()
    for pt in [(100.4,100.4),(153.6,100.4),(153.6,154.6),(100.4,154.6)]: poly.Append(V(*pt))
    board.Add(z)

# 54 x 55 mm: inside the 56.34 x 98 mm LCD PCB footprint.
for a,b in zip([(100,100),(154,100),(154,155),(100,155)],[(154,100),(154,155),(100,155),(100,100)]): edge(a,b)

# Screen socket. Pin 1 is the top end; LCD header order is VCC through T_IRQ.
j1=fp("Connector_PinSocket_2.54mm.pretty","PinSocket_1x14_P2.54mm_Vertical","J1","LCD_ILI9488_XPT2046",151.5,109.0)
nets(j1,{1:"+5V",2:"GND",3:"TFT_CS",4:"TFT_RST",5:"TFT_DC",6:"TFT_MOSI",7:"TFT_SCK",8:"TFT_LED",10:"TOUCH_CLK",11:"TOUCH_CS",12:"TOUCH_DIN",13:"TOUCH_DO",14:"TOUCH_IRQ"})

# Exact V1A EXP mappings; these are adapter-side male 2x5 headers.
j2=fp("Connector_PinHeader_2.54mm.pretty","PinHeader_2x05_P2.54mm_Vertical","J2","EXP1_TO_V1A",104.5,105.5)
nets(j2,{1:"TOUCH_DO",2:"TOUCH_CS",3:"TFT_CS",4:"TFT_DC",5:"TFT_RST",6:"TOUCH_IRQ",9:"GND",10:"+5V"})
j3=fp("Connector_PinHeader_2.54mm.pretty","PinHeader_2x05_P2.54mm_Vertical","J3","EXP2_TO_V1A",104.5,128.0)
nets(j3,{2:"TFT_SCK",3:"TOUCH_DIN",5:"TOUCH_CLK",6:"TFT_MOSI",9:"GND"})

# Only added V1A harness: pin 1=3V3, pin 2=GND, pin 3=GP1 PWM.
j4=fp("Connector_JST.pretty","JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical","J4","AUX_3V3_GND_GP1",124.0,147.0)
nets(j4,{1:"+3V3",2:"GND",3:"GP1_BL_PWM"})

# Si4599 P-MOS high-side backlight circuit: GP1 -> R1 -> gate and R2 pulls
# gate to source, leaving the display dark during Pico reset.  N MOSFET is NC.
q1=fp("Package_SO.pretty","SOIC-8_3.9x4.9mm_P1.27mm","Q1","Si4599DY",125.0,105.0)
nets(q1,{4:"BL_GATE",5:"+3V3",6:"+3V3",7:"TFT_LED",8:"TFT_LED"})
r1=fp("Resistor_SMD.pretty","R_0603_1608Metric","R1","1k",117.5,106.8); nets(r1,{1:"GP1_BL_PWM",2:"BL_GATE"})
r2=fp("Resistor_SMD.pretty","R_0603_1608Metric","R2","100k",116.0,112.0); nets(r2,{1:"BL_GATE",2:"+3V3"})

label("Hank Touchscreen Adapter",135,115.4,.95,.18)
label("REV A  |  6 LAYER",135,116.8,.80,.14)
label("LCD J1",146.0,106.8,.78,.14,90); label("1▲",150.2,107.0,.85,.15,90)
label("EXP1",109.0,102.6,.95,.17); label("1▲",101.8,105.5,.75,.13)
label("EXP2",109.0,125.1,.95,.17); label("1▲",101.8,128.0,.75,.13)
label("AUX",124.0,143.5,.95,.17); label("1=3V3  2=GND  3=GP1/BL",127,151.7,.80,.14)

F,B,I1,I2,I3,I4=pcbnew.F_Cu,pcbnew.B_Cu,pcbnew.In1_Cu,pcbnew.In2_Cu,pcbnew.In3_Cu,pcbnew.In4_Cu
# Inner-layer signal fanout: independent, point-to-point corridors.
track("TOUCH_DO",[xy(j2,1),(102.0,105.5),(102.0,101.2),(149.0,101.2),(149.0,xy(j1,13)[1]),xy(j1,13)],F)
track("TOUCH_IRQ",[xy(j2,6),(110.0,110.58),(110.0,153.5),(148.0,153.5),(148.0,xy(j1,14)[1]),xy(j1,14)],F)
track("TOUCH_CS",[xy(j2,2),xy(j1,11)],B)
track("TOUCH_DIN",[xy(j3,3),(102.8,130.54),(102.8,124.0),xy(j1,12)],B)
track("TOUCH_CLK",[xy(j3,5),(102.0,133.08),(102.0,102.2),(153.0,102.2),(153.0,xy(j1,10)[1]),xy(j1,10)],B)
track("TFT_CS",[xy(j2,3),(102.0,108.04),(102.0,121.5),xy(j1,3)],I3)
track("TFT_DC",[xy(j2,4),xy(j1,5)],I4)
track("TFT_RST",[xy(j2,5),(101.4,110.58),(101.4,150.0),(153.0,150.0),(153.0,xy(j1,4)[1]),xy(j1,4)],I4)
track("TFT_SCK",[xy(j3,2),xy(j1,7)],I4)
track("TFT_MOSI",[xy(j3,6),(109.0,133.08),(109.0,152.0),(150.0,152.0),(150.0,xy(j1,6)[1]),xy(j1,6)],I2)

# Supply and local decoupling.
track("+5V",[xy(j2,10),(108.5,115.66),(108.5,101.0),(150.0,101.0),(150.0,xy(j1,1)[1]),xy(j1,1)],I2,.60)

# Backlight circuit. TFT LED current never enters GP1.
track("+3V3",[xy(j4,1),(124.0,144.0),(132.0,144.0),(132.0,105.635),xy(q1,6)],F,.50)
track("+3V3",[xy(q1,6),xy(q1,5)],F,.50)
track("+3V3",[xy(r2,2),(116.825,114.0),(130.0,114.0),(130.0,105.635),xy(q1,6)],F)
track("GP1_BL_PWM",[xy(j4,3),(129.0,149.0),(114.0,149.0),(114.0,106.8),xy(r1,1)],F)
track("BL_GATE",[xy(r1,2),(121.0,106.8),xy(q1,4)],F)
track("BL_GATE",[xy(r2,1),(115.175,110.5),(120.0,110.5),(120.0,xy(q1,4)[1]),xy(q1,4)],F)
track("TFT_LED",[xy(q1,7),xy(q1,8)],F,.50)
track("TFT_LED",[xy(q1,8),(140.0,103.1)],F,.50)
via("TFT_LED",140.0,103.1,F,I1)
track("TFT_LED",[(140.0,103.1),(140.0,xy(j1,8)[1]),xy(j1,8)],I1,.50)

# Ground plane connections are deliberately plane-based, with short return
# paths on both external layers.
zone(F); zone(B)
pcbnew.ZONE_FILLER(board).Fill(board.Zones())
board.Save(str(OUT))
print(OUT)
