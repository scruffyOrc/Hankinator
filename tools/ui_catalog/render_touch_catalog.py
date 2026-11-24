#!/usr/bin/env python3
"""Render a source-aligned visual catalog of Hank's 480x320 touch UI.

This intentionally runs on a workstation, not on the Pico.  It mirrors the
production canvas, palette, common geometry, labels, and representative live
states from ``src/touch_diagnostics_ui.cpp`` without adding any firmware RAM
or flash cost.  It is a review artifact; the physical display remains the
authority for final pixel-level verification.

Run from the repository root:
    py tools/ui_catalog/render_touch_catalog.py
"""
from __future__ import annotations

from pathlib import Path
import re
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs" / "ui-catalog" / "screens"
W, H = 480, 320

BACKGROUND = "#F3F5F9"; HEADER = "#292D43"; INK = "#20243A"; MUTED = "#73788D"
BLUE = "#4856C8"; BLUE_LIGHT = "#6170D8"; TEAL = "#11998E"; TEAL_LIGHT = "#21A89B"
GOLD = "#A96508"; PURPLE = "#6B5CC6"; RED = "#B84555"; DANGER = "#B61E28"; SUCCESS = "#2FAF5A"
WHITE = "#FFFFFF"; DARK = "#090B12"; TRACK = "#D9DEEA"; SHADOW = "#C8CDD9"

FONT_DIR = Path("C:/Windows/Fonts")
def font(size: int, bold: bool = False):
    names = ["segoeuib.ttf", "segoeuisb.ttf"] if bold else ["segoeui.ttf"]
    for name in names:
        path = FONT_DIR / name
        if path.exists(): return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()

def draw_splash_title(im):
    """Use the exact embedded Playwrite bitmap used before LVGL starts."""
    source = (ROOT / "include" / "playwrite_splash_title.h").read_text(encoding="utf-8")
    width = int(re.search(r"Width=(\d+)", source).group(1))
    height = int(re.search(r"Height=(\d+)", source).group(1))
    row_bytes = int(re.search(r"RowBytes=(\d+)", source).group(1))
    bits = [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", source)]
    pixels = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    px = pixels.load()
    for y in range(height):
        for x in range(width):
            if bits[y * row_bytes + x // 8] & (1 << (x & 7)):
                px[x, y] = (255, 255, 255, 255)
    im.paste(pixels, ((W - width) // 2, 18), pixels)

def canvas(dark=False):
    return Image.new("RGB", (W, H), DARK if dark else BACKGROUND)

def center(draw, text, xy, size=16, color=INK, bold=False, anchor="mm"):
    draw.multiline_text(xy, text, font=font(size, bold), fill=color, anchor=anchor,
                        align="center", spacing=1)

def header(im, title, back=True, dark=False):
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, W, 70), radius=0, fill=HEADER)
    if back:
        d.rounded_rectangle((12, 12, 64, 58), radius=12, outline="#535973", width=2)
        center(d, "‹", (38, 34), 42, WHITE)
        x = 84
    else: x = 20
    d.text((x, 24), title, font=font(20, True), fill=WHITE)

def card(draw, label, box, color, sub=None, disabled=False):
    x, y, w, h = box; fill = "#B8BCC7" if disabled else color
    draw.rounded_rectangle((x+2,y+3,x+w+2,y+h+3), 16, fill=SHADOW)
    draw.rounded_rectangle((x,y,x+w,y+h), 16, fill=fill)
    center(draw, label, (x+w/2, y+h/2-(7 if sub else 0)), 17, WHITE, True)
    if sub: center(draw, sub, (x+w/2,y+h/2+18), 12, WHITE)

def button(draw, label, box, color=BLUE):
    x,y,w,h=box; draw.rounded_rectangle((x,y,x+w,y+h), 14, fill=color)
    center(draw,label,(x+w/2,y+h/2),17,WHITE,True)

def toggle(draw, x, y, on=True):
    draw.rounded_rectangle((x,y,x+58,y+30), 15, fill=TEAL if on else "#BFC5D2")
    r=12; cx=x+43 if on else x+15
    draw.ellipse((cx-r,y+15-r,cx+r,y+15+r), fill=WHITE)

def gauge(draw, cx=240, cy=178, pct=42, color=TEAL):
    # Production: 220 px LVGL arc at (130,72), 18 px stroke, 135..405 degrees.
    box=(cx-110,cy-110,cx+110,cy+110); draw.arc(box, 135, 405, fill="#D3D9E8", width=18)
    draw.arc(box, 135, 135+270*pct/100, fill=color, width=18)

def save(name, im):
    OUT.mkdir(parents=True, exist_ok=True); im.save(OUT / f"{name}.png")

def simple(name, title, main, detail="", back=True, action=None, color=BLUE, warning=False):
    im=canvas(); header(im,title,back); d=ImageDraw.Draw(im)
    if warning: center(d,"!",(240,112),44,DANGER,True)
    center(d,main,(240,142 if warning else 134),24,INK,True)
    if detail: center(d,detail,(240,180 if warning else 170),15,MUTED)
    if action: button(d,action,(26,250,428,48),color)
    save(name,im)

def home():
    im=canvas(); header(im,"Hank Winder",False); d=ImageDraw.Draw(im)
    card(d,"WEIGHT",(20,82,210,104),TEAL); card(d,"COUNT",(250,82,210,104),BLUE)
    card(d,"HAND WIND",(20,202,210,104),PURPLE); card(d,"SCALE",(250,202,210,104),GOLD)
    d.text((427,20),"⚙",font=font(26),fill=WHITE); save("01_home",im)

def menus():
    im=canvas(); header(im,"Configuration"); d=ImageDraw.Draw(im)
    card(d,"WEIGHT TARGETS",(20,88,210,92),TEAL); card(d,"CONNECTIVITY",(250,88,210,92),RED)
    card(d,"MACHINE SETTINGS",(20,198,210,92),BLUE); card(d,"DIAGNOSTICS",(250,198,210,92),GOLD); save("02_configuration",im)
    im=canvas(); header(im,"Machine settings"); d=ImageDraw.Draw(im)
    card(d,"COMMISSION",(20,88,210,92),BLUE); card(d,"MOTOR",(250,88,210,92),BLUE_LIGHT)
    card(d,"HALL FEEDBACK",(20,198,210,92),BLUE_LIGHT,"Enabled"); card(d,"DISPLAY",(250,198,210,92),BLUE); save("03_machine_settings",im)
    im=canvas(); header(im,"Diagnostics"); d=ImageDraw.Draw(im)
    card(d,"SYSTEM CHECK",(20,88,210,92),GOLD); card(d,"WEIGHT",(250,88,210,92),GOLD)
    card(d,"FACTORY RESET",(20,198,440,92),DANGER); save("04_diagnostics",im)
    im=canvas(); header(im,"Weight targets"); d=ImageDraw.Draw(im)
    card(d,"MINI",(20,88,135,160),TEAL,"20 g"); card(d,"HALF",(173,88,135,160),TEAL_LIGHT,"50 g"); card(d,"FULL",(326,88,135,160),"#46B7AC","100 g"); save("05_weight_targets",im)

def target_editor():
    im=canvas(); header(im,"Mini target"); d=ImageDraw.Draw(im)
    center(d,"20.0 g",(215,128),36,INK,True); button(d,"⌨",(332,101,58,54),TEAL)
    button(d,"−",(57,205,92,56),TEAL); button(d,"RESET",(194,205,92,56),TEAL_LIGHT); button(d,"+",(331,205,92,56),TEAL); save("06_weight_target_editor",im)

def display():
    im=canvas(); header(im,"Display"); d=ImageDraw.Draw(im)
    d.text((28,102),"Dark mode",font=font(18,True),fill=INK); toggle(d,392,91,False)
    card(d,"TOUCH CALIBRATION",(20,145,210,92),BLUE); card(d,"DISPLAY SLEEP",(250,145,210,92),BLUE_LIGHT); save("07_display",im)
    im=canvas(); header(im,"Display sleep"); d=ImageDraw.Draw(im)
    d.text((28,98),"Blank display",font=font(18,True),fill=INK); toggle(d,392,87,True)
    center(d,"5 minutes",(215,165),30,INK,True); button(d,"⌨",(334,138,55,52),BLUE)
    button(d,"−",(65,220,90,48),BLUE); button(d,"+",(325,220,90,48),BLUE); save("08_display_sleep",im)

def motor():
    im=canvas(); header(im,"Motor"); d=ImageDraw.Draw(im)
    card(d,"RUN CURRENT",(20,88,135,150),BLUE,"850 mA"); card(d,"HOLD CURRENT",(173,88,135,150),BLUE_LIGHT,"300 mA"); card(d,"DIRECTION",(326,88,135,150),"#5261C8","Clockwise"); save("09_motor",im)
    simple("10_current_warning","Run current","Changing this value can damage\nthe motor or machine.","Proceed?",True,"YES",DANGER,True)
    im=canvas(); header(im,"Run current"); d=ImageDraw.Draw(im); center(d,"850 mA",(214,128),34,INK,True); button(d,"⌨",(337,102,55,52),BLUE); button(d,"−",(57,205,92,56),BLUE); button(d,"RESET",(194,205,92,56),BLUE_LIGHT); button(d,"+",(331,205,92,56),BLUE); save("11_run_current",im)
    simple("12_motor_direction","Rotation direction","CLOCKWISE", "", True, "COUNTER CLOCKWISE", BLUE)

def connectivity():
    im=canvas(); header(im,"Connectivity"); d=ImageDraw.Draw(im)
    d.text((28,100),"Bluetooth",font=font(20,True),fill=INK); d.text((28,126),"Connected and available",font=font(14),fill=MUTED); toggle(d,392,96,True)
    button(d,"PAIR DEVICE",(26,180,250,52),RED); save("13_connectivity",im)
    im=canvas(); header(im,"Connectivity"); d=ImageDraw.Draw(im); d.text((28,100),"Bluetooth",font=font(20,True),fill=INK); toggle(d,392,96,True); button(d,"STOP PAIRING",(26,180,250,52),RED); center(d,"58 s",(380,206),20,INK,True); center(d,"Pair your device now",(240,270),16,MUTED); save("14_pairing",im)

def diagnostics():
    simple("15_system_check","System check","All checks passed", "Load cells, Hall sensor, motor, stop button", True, "DONE", SUCCESS)
    im=canvas(); header(im,"Weight diagnostics"); d=ImageDraw.Draw(im); center(d,"48.1 g",(240,140),48,INK,True); center(d,"Stable live weight",(240,184),16,MUTED); button(d,"TARE",(26,250,428,48),GOLD); save("16_weight_diagnostics",im)
    im=canvas(); header(im,"Scale"); d=ImageDraw.Draw(im); center(d,"333.5",(240,134),58,INK,True); center(d,"g",(240,184),24,MUTED); button(d,"UNIT: g",(26,224,200,48),GOLD); button(d,"TARE",(254,224,200,48),GOLD); save("17_scale",im)

def commissioning():
    simple("18_commission_ready","Commission load cells","Distributed collar calibration","All 4 collars: front, middle, rear",True,"BEGIN",GOLD)
    simple("19_commission_leg","Commission load cells","79.43 g over FL leg","Direct capture 1 of 12",True,"CAPTURE",GOLD)
    simple("20_commission_arm","Commission load cells","All 4 collars at middle","Distributed capture 2 of 3",True,"CAPTURE",GOLD)
    simple("21_commission_complete","Commission load cells","Calibration saved","0 deg 192.3 g | 180 deg 192.4 g",True,"DONE",SUCCESS)

def reset():
    simple("22_factory_reset","Factory reset","All settings will be removed.","Commissioning data remains unless enabled below.",True,"RESET",DANGER,True)
    im=canvas(); header(im,"Factory reset"); d=ImageDraw.Draw(im); center(d,"Enter code to reset\ncommissioning data",(240,105),21,INK,True); center(d,"• • • • • •",(240,163),28,INK); button(d,"1",(90,196,70,38),BLUE); button(d,"2",(205,196,70,38),BLUE); button(d,"3",(320,196,70,38),BLUE); save("23_factory_reset_code",im)
    simple("24_factory_reset_confirm","Factory reset","Are you sure?","This cannot be undone.",True,"YES",DANGER,True)

def winding():
    im=canvas(); header(im,"Weight winding"); d=ImageDraw.Draw(im); card(d,"MINI",(20,100,135,150),TEAL,"20 g"); card(d,"HALF",(173,100,135,150),TEAL_LIGHT,"50 g"); card(d,"FULL",(326,100,135,150),"#46B7AC","100 g"); save("25_weight_select",im)
    simple("26_load_yarn","Weight winding","Load yarn on Hank","Then press GO. Hank will find home, tare, and begin.",True,"GO",TEAL)
    im=canvas(); header(im,"Weight winding"); d=ImageDraw.Draw(im); gauge(d,240,182,42,TEAL); d.ellipse((177,119,303,245),fill=BLUE); center(d,"PAUSE",(240,182),17,WHITE,True); center(d,"EST. 42%",(240,294),20,TEAL,True); d.text((362,132),"TURNS\n34.3",font=font(16),fill=MUTED,spacing=2); d.text((362,202),"MEASURED\n16.8 g",font=font(16),fill=MUTED,spacing=2); save("27_weight_winding",im)
    im=canvas(); header(im,"Weight winding"); d=ImageDraw.Draw(im); gauge(d,240,182,42,TEAL); d.ellipse((177,119,303,245),fill=BLUE); center(d,"RESUME",(240,182),17,WHITE,True); center(d,"42%",(240,294),20,TEAL,True); d.text((362,132),"TURNS\n34.3",font=font(16),fill=MUTED,spacing=2); d.text((362,202),"MEASURED\n16.8 g",font=font(16),fill=MUTED,spacing=2); save("28_weight_paused",im)
    simple("29_weight_complete","Weight winding","Complete","21.7 g measured | 43.00 turns",True,"+1 TURN",TEAL)
    im=canvas(); header(im,"Count winding"); d=ImageDraw.Draw(im); center(d,"62",(214,130),52,INK,True); center(d,"turns",(214,177),18,MUTED); button(d,"⌨",(335,103,55,52),BLUE); button(d,"START",(26,250,428,48),BLUE); save("30_count_select",im)
    im=canvas(); header(im,"Count winding"); d=ImageDraw.Draw(im); gauge(d,240,182,58,BLUE); d.ellipse((177,119,303,245),fill=BLUE); center(d,"PAUSE",(240,182),17,WHITE,True); center(d,"58%",(240,294),20,BLUE,True); d.text((362,132),"TURNS\n36.0",font=font(16),fill=MUTED,spacing=2); d.text((362,202),"MEASURED\n18.2 g",font=font(16),fill=MUTED,spacing=2); save("31_count_winding",im)
    simple("32_stop_confirm","Stop winding?","Winding will safely coast to a stop.","You can resume by selecting No.",True,"YES",DANGER,True)

def hand_turning():
    im=canvas(); header(im,"Hand turning"); d=ImageDraw.Draw(im); center(d,"12.25",(240,126),54,INK,True); center(d,"turns",(240,174),21,MUTED); center(d,"18.7 g",(240,211),24,INK,True); button(d,"DONE",(26,250,428,48),PURPLE); save("33_hand_turning",im)

def dark_mode():
    im=canvas(True); header(im,"Display",True,True); d=ImageDraw.Draw(im); d.text((28,102),"Dark mode",font=font(18,True),fill=WHITE); toggle(d,392,91,True); card(d,"TOUCH CALIBRATION",(20,145,210,92),BLUE); card(d,"DISPLAY SLEEP",(250,145,210,92),BLUE_LIGHT); save("34_dark_mode",im)

def transitional():
    im=canvas(True); d=ImageDraw.Draw(im)
    draw_splash_title(im)
    # Exact geometry from drawWinderMark(): rimmed hub, four offset arms,
    # tripod and offset drive. This avoids the generic spinner used before.
    hx,hy=244,132; cyan="#00D7E9"; frame="#D3D3D3"
    d.ellipse((hx-49,hy-49,hx+49,hy+49),outline=frame,width=2); d.ellipse((hx-46,hy-46,hx+46,hy+46),outline=frame,width=2)
    for dx,dy in ((-55,-55),(55,-55),(55,55),(-55,55)):
        d.line((hx,hy,hx+dx,hy+dy),fill=cyan,width=2); d.line((hx+2,hy,hx+dx+2,hy+dy),fill=cyan,width=2); d.ellipse((hx+dx-8,hy+dy-8,hx+dx+8,hy+dy+8),fill=cyan); d.ellipse((hx+dx-4,hy+dy-4,hx+dx+4,hy+dy+4),fill=DARK)
    d.ellipse((hx-17,hy-17,hx+17,hy+17),fill=cyan); d.ellipse((hx-7,hy-7,hx+7,hy+7),fill=DARK)
    d.line((hx-20,hy+38,158,238),fill=frame,width=2); d.line((hx-17,hy+38,161,238),fill=frame,width=2); d.line((hx+22,hy+38,326,238),fill=frame,width=2); d.line((hx+19,hy+38,323,238),fill=frame,width=2); d.line((145,239,176,239),fill=frame,width=2); d.line((311,239,342,239),fill=frame,width=2)
    d.ellipse((hx-73,hy+40,hx-47,hy+66),outline=cyan,width=2); d.line((hx-48,hy+45,hx-30,hy+30),fill=cyan,width=2); d.line((hx-51,hy+61,hx-28,hy+49),fill=cyan,width=2); d.rounded_rectangle((hx-96,hy+51,hx-66,hy+73),5,fill=cyan); d.ellipse((hx-65,hy+48,hx-55,hy+58),fill=DARK)
    d.arc((hx-58,hy-58,hx+58,hy+58),278,340,fill="#FFE600",width=8)
    center(d,"Checking hardware",(240,280),16,"#D3D3D3"); save("35_startup",im)
    im=canvas(); header(im,"Touch calibration"); d=ImageDraw.Draw(im); center(d,"Touch each target as it appears",(240,105),19,INK,True); d.ellipse((222,145,258,181),outline=BLUE,width=4); d.line((240,137,240,189),fill=BLUE,width=3); d.line((214,163,266,163),fill=BLUE,width=3); save("36_touch_calibration",im)
    simple("37_commission_failed","Commission load cells","Commission failed","First validation is outside tolerance.",True,"BACK TO SETTINGS",DANGER)
    simple("38_factory_reset_complete","Factory reset","Reset complete","The system must restart now.",False,"RESTART",SUCCESS)
    simple("39_weight_estimate_failed","Weight winding","Unable to estimate winding duration","16 turns completed. Wind by hand or use Count mode.",True,"DONE",DANGER,True)
    im=canvas(True); d=ImageDraw.Draw(im); center(d,"BEHOLD!!!",(240,148),58,"#FF3A4C",True); save("40_easter_egg_behold",im)
    im=canvas(True); d=ImageDraw.Draw(im); center(d,"Hank",(240,116),46,WHITE,True); center(d,"Manufactured by",(240,177),24,TEAL_LIGHT); save("41_easter_egg_credit",im)
    im=canvas(True); d=ImageDraw.Draw(im); center(d,"BuzzyToes",(240,125),42,WHITE,True); center(d,"Creations",(240,176),42,WHITE,True); center(d,"© 2026",(425,290),13,WHITE); save("42_easter_egg_logo",im)

def render_catalog(destination: Path, is_dark: bool):
    global OUT, BACKGROUND, INK, MUTED, TRACK, SHADOW
    OUT = destination
    BACKGROUND, INK, MUTED, TRACK, SHADOW = (
        ("#090B12", "#F3F5F9", "#A8B0C4", "#30374A", "#030407") if is_dark else
        ("#F3F5F9", "#20243A", "#73788D", "#D9DEEA", "#C8CDD9")
    )
    home(); menus(); target_editor(); display(); motor(); connectivity(); diagnostics(); commissioning(); reset(); winding(); hand_turning(); dark_mode(); transitional()
    images = sorted(p.name for p in OUT.glob("*.png"))
    thumb_w, thumb_h, row_h = 240, 160, 188
    contact = Image.new("RGB", (thumb_w * 2, row_h * ((len(images) + 1) // 2)), WHITE)
    contact_draw = ImageDraw.Draw(contact)
    for number, filename in enumerate(images):
        image = Image.open(OUT / filename).resize((thumb_w, thumb_h))
        x, y = number % 2 * thumb_w, number // 2 * row_h
        contact.paste(image, (x, y))
        contact_draw.text((x + 6, y + thumb_h + 5), filename, font=font(10), fill=INK)
    contact.save(OUT.parent / ("dark-contact-sheet.png" if is_dark else "contact-sheet.png"))
    return images

def main():
    light = render_catalog(ROOT / "docs" / "ui-catalog" / "screens", False)
    dark = render_catalog(ROOT / "docs" / "ui-catalog" / "dark-screens", True)
    index = ROOT / "docs" / "ui-catalog" / "README.md"; index.parent.mkdir(parents=True, exist_ok=True)
    index.write_text("# Hank touchscreen UI catalog\n\nNative 480 x 320 source-aligned review renders. They are produced outside the firmware, so they cost Hank no flash or RAM. Regenerate with `C:\\Users\\crisd\\.cache\\codex-runtimes\\codex-primary-runtime\\dependencies\\python\\python.exe tools\\ui_catalog\\render_touch_catalog.py`.\n\n[Light-mode contact sheet](contact-sheet.png)\n\n[Dark-mode contact sheet](dark-contact-sheet.png)\n\n## Light mode\n\n" + "\n".join(f"- [screens/{p}](screens/{p})" for p in light) + "\n\n## Dark mode\n\n" + "\n".join(f"- [dark-screens/{p}](dark-screens/{p})" for p in dark) + "\n", encoding="utf-8")
    print(f"Rendered {len(light)} light and {len(dark)} dark screens")

if __name__ == "__main__": main()
