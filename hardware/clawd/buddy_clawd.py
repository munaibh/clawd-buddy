"""Clawd-shaped case for the Waveshare ESP32-S3-Touch-LCD-1.46B + a 503035 LiPo.

Run with CadQuery:  python buddy_clawd.py   -> writes STLs next to this file.

The board is mounted upside down so USB-C comes out of the top of Clawd's
head: flash the waveshare-146b-flipped firmware.

Frame: z = 0 is the front of the glass, the body goes back to -z. x right and
y up as you look at Clawd's face. Board features from Waveshare's STEP model
are given in the board's own frame and turned 180 degrees here.

Parts
  shell    body, arms, legs; glass flush in the face; angled button tunnels
  plate    sits on the inner ledge; the board screws to its posts (3 x M2x6)
  back     flat back cover, 4 x M2x6 into the corner bosses; optional key clip
  plunger  side-button plungers (PWR and BOOT), push in from outside after the board
  key      or press the buttons with this, like a SIM ejector
  spacer   frame round the battery between plate and back: holds it all snug
"""
import math
import os

import cadquery as cq

OUT = os.path.dirname(os.path.abspath(__file__))

CLR = 0.25
R_GLASS = 22.39
R_POCKET = R_GLASS + CLR
RIM = 0.3                    # face stands this proud of the glass
EDGE = 0.8                   # outer edge chamfer

U = 6.0                      # Clawd "pixel"
BODY_W, BODY_H = 10 * U, 9 * U          # 60 x 54
ARM_Y = (-5.0, 1.0)          # arm band (y range), one pixel tall
LEGS_X = (-21, -9, 9, 21)    # leg centres: a pair each side
WALL = 1.6

# board stack (z): glass/LCD front section, then the open cavity
Z_GLASS_BACK = -4.7
Z_PLATE_TOP = -13.2          # pin header is the deepest board part at -12.95
PLATE_T = 1.2
BATT_T = 5.0 + 0.3
BACK_T = 1.6
Z_PLATE_BOT = Z_PLATE_TOP - PLATE_T
Z_BACK = Z_PLATE_BOT - BATT_T - BACK_T
CAV = BODY_W / 2 - WALL - 1.6           # cavity half-width; 1.6 mm ledge round the bay
BAY = BODY_W / 2 - WALL                 # battery bay half-width
BAY_H = BODY_H / 2 - WALL
CAV_H = BAY_H - 1.6

POST_R, HEAD_R, HEAD_H = 2.8, 2.15, 1.7
BOSS_R, BOSS_HOLE = 3.0, 0.95           # back-cover screws self-tap (M2): 1.9 mm, prints ~1.75
BOSS_AT = [(sx * (BAY - 2.6), sy * (BAY_H - 2.6)) for sx in (-1, 1) for sy in (-1, 1)]


def flip(x, y):
    """Board frame -> Clawd frame (board turned 180 degrees)."""
    return -x, -y


NUTS = [flip(x, y) for x, y in [(12.0, -14.95), (-11.54, -15.45), (0.0, 17.75)]]
Z_NUT = -10.91
USB_Z = -8.64
BAT_PLUG = flip(7.5, 14.6)


def _box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))


def outer():
    """Clawd's silhouette, extruded front to back, edges softened."""
    zt = RIM - Z_BACK
    w = _box(-BODY_W / 2, BODY_W / 2, -BODY_H / 2, BODY_H / 2, Z_BACK, RIM)
    for sx in (-1, 1):
        x0 = BODY_W / 2 if sx > 0 else -BODY_W / 2 - U
        w = w.union(_box(x0, x0 + U, ARM_Y[0], ARM_Y[1], Z_BACK, RIM))
    for x in LEGS_X:
        w = w.union(_box(x - U / 2, x + U / 2, -BODY_H / 2 - U, -BODY_H / 2 + 0.01, Z_BACK, RIM))
    w = w.clean()
    return w.edges("|Z").chamfer(EDGE).faces(">Z or <Z").edges().chamfer(EDGE)


def shell():
    s = outer()
    # glass sits flush in a round recess; behind it an open cavity
    s = s.cut(cq.Workplane("XY").workplane(offset=Z_GLASS_BACK).circle(R_POCKET).extrude(RIM - Z_GLASS_BACK + 1))
    s = s.cut(_box(-CAV, CAV, -CAV_H, CAV_H, Z_PLATE_TOP, Z_GLASS_BACK + 0.01))
    # battery bay behind the ledge, open at the back for the cover
    s = s.cut(_box(-BAY, BAY, -BAY_H, BAY_H, Z_BACK - 1, Z_PLATE_TOP + 0.01))
    # display flex cable and the board's flat tab (board bottom = Clawd top)
    s = s.cut(_box(-7.35, 8.15, 21.0, 23.6, -4.1, -1.7))
    s = s.cut(_box(-10.3, 10.3, 19.4, 22.4, -7.7, -2.3))
    # USB-C out of the top of the head
    s = s.cut(cq.Workplane("XZ").workplane(offset=-20).center(0, USB_Z)
              .rect(12.6, 7.2).extrude(-15).edges("|Y").fillet(2.5))
    # speaker vents on the left side, above the arm
    for y in (3.0, 4.8, 6.6):
        s = s.cut(_box(-BODY_W / 2 - 1, -CAV + 0.1, y - 0.5, y + 0.5, -12.6, -8.6))
    # corner bosses for the back cover
    for x, y in BOSS_AT:
        s = s.union(cq.Workplane("XY").workplane(offset=Z_BACK + BACK_T).center(x, y)
                    .circle(BOSS_R).extrude(Z_PLATE_BOT - Z_BACK - BACK_T - 0.2))
        s = s.cut(cq.Workplane("XY").workplane(offset=Z_BACK).center(x, y).circle(BOSS_HOLE).extrude(8))
    # side-button tunnels (see BUTTONS below)
    body = outer()
    for ang, r_stem in STEMS:
        r_g = r_stem + GUIDE_GAP
        guide = _box(r_g, r_g + 14, -GUIDE_W / 2, GUIDE_W / 2, Z_STEM - 2.0, Z_GLASS_BACK + 0.01)
        s = s.union(_rot(guide, ang).intersect(body))
        s = s.cut(_rot(_box(r_g - 0.01, 50, -TUN_W / 2, TUN_W / 2, Z_STEM - TUN_H / 2, Z_STEM + TUN_H / 2), ang))
    return s


def plate():
    p = _box(-BAY + CLR, BAY - CLR, -BAY_H + CLR, BAY_H - CLR, Z_PLATE_BOT, Z_PLATE_TOP)
    for x, y in BOSS_AT:                                     # clear the bosses
        p = p.cut(cq.Workplane("XY").workplane(offset=Z_PLATE_BOT - 1).center(x, y).circle(BOSS_R + CLR).extrude(5))
    for x, y in NUTS:
        p = p.union(cq.Workplane("XY").workplane(offset=Z_PLATE_TOP).center(x, y)
                    .circle(POST_R).extrude(Z_NUT - Z_PLATE_TOP - 0.05))
        p = p.cut(cq.Workplane("XY").workplane(offset=Z_PLATE_BOT - 1).center(x, y).circle(1.15).extrude(10))
        p = p.cut(cq.Workplane("XY").workplane(offset=Z_PLATE_BOT - 1).center(x, y).circle(HEAD_R).extrude(1 + HEAD_H))
    p = p.cut(cq.Workplane("XY").workplane(offset=Z_PLATE_BOT - 1).center(*BAT_PLUG).slot2D(9, 4.5).extrude(4))
    return p


def back(key_clip=False):
    """Flat back cover; the battery is taped to it, any way round.
    key_clip=True adds the slide-in holder for key() on the outside (print
    that one outside up)."""
    b = _box(-BAY + CLR, BAY - CLR, -BAY_H + CLR, BAY_H - CLR, Z_BACK, Z_BACK + BACK_T)
    if key_clip:
        b = b.union(_key_clip())
    for x, y in BOSS_AT:
        b = b.cut(cq.Workplane("XY").workplane(offset=Z_BACK - 1).center(x, y).circle(1.15).extrude(5))
        b = b.cut(cq.Workplane("XY").workplane(offset=Z_BACK - 1).center(x, y).circle(2.2).extrude(1 + 0.8))
    return b


# SPACER: a frame a touch thicker than the battery gap, so screwing on the
# back clamps the plate to the ledge (the cell alone doesn't fill the gap).
SPACER_SQUEEZE = 0.1                 # thicker than the gap by this much
SPACER_RAIL = 2.0                    # border width
SPACER_RING = 1.2                    # wall round each corner post


def spacer(squeeze=SPACER_SQUEEZE):
    t = BATT_T + squeeze
    z0 = Z_BACK + BACK_T
    x1, y1 = BAY - CLR, BAY_H - CLR
    sp = _box(-x1, x1, -y1, y1, z0, z0 + t)
    sp = sp.cut(_box(-x1 + SPACER_RAIL, x1 - SPACER_RAIL, -y1 + SPACER_RAIL, y1 - SPACER_RAIL, z0 - 1, z0 + t + 1))
    for x, y in BOSS_AT:
        sp = sp.union(cq.Workplane("XY").workplane(offset=z0).center(x, y).circle(BOSS_R + CLR + SPACER_RING).extrude(t)
                      .intersect(_box(-x1, x1, -y1, y1, z0, z0 + t)))
        sp = sp.cut(cq.Workplane("XY").workplane(offset=z0 - 1).center(x, y).circle(BOSS_R + CLR).extrude(t + 2))
    return cq.Workplane("XY").add(max(sp.solids().vals(), key=lambda v: v.Volume()))   # drop zero-volume slivers


# BUTTON KEY: a SIM-ejector pin for the button tunnels when no plunger is
# fitted; the switch is about 17 mm in.
KEY_T = 1.2                         # thickness, pin and grip alike
KEY_PIN_W, KEY_PIN_L = 1.2, 20.0
KEY_GRIP_W, KEY_GRIP_L = 8.0, 10.0
KEY_CLIP_L = KEY_PIN_L + KEY_GRIP_L + 1.0


def key():
    k = _box(0, KEY_GRIP_L, -KEY_GRIP_W / 2, KEY_GRIP_W / 2, 0, KEY_T).edges("|Z").fillet(1.5)
    k = k.cut(cq.Workplane("XY").center(KEY_GRIP_L / 2 - 1, 0).circle(1.6).extrude(KEY_T))   # finger hole / lanyard
    k = k.union(_box(KEY_GRIP_L - 0.5, KEY_GRIP_L + KEY_PIN_L, -KEY_PIN_W / 2, KEY_PIN_W / 2, 0, KEY_T))
    return k.edges("|Z and >X").chamfer(0.3)


def _key_clip():
    """Two rails with lips on the outside of the back: the key slides in
    pin first from the +x end and stops at the -x end. A slight bump under
    the grip holds it by friction."""
    z0 = Z_BACK                      # outside face of the back
    gap = KEY_T + 0.15               # slot height under the lips
    lip = 0.8
    w_in = KEY_GRIP_W + 0.4          # channel width at the base
    x0, x1 = -KEY_CLIP_L / 2, KEY_CLIP_L / 2
    c = cq.Workplane("XY")
    for sy in (-1, 1):
        y_in = sy * w_in / 2
        y_out = sy * (w_in / 2 + 1.2)
        ya, yb = sorted((y_in, y_out))
        c = c.union(_box(x0, x1, ya, yb, z0 - gap - lip, z0 + 0.01))               # rail
        la, lb = sorted((y_in, y_in - sy * 1.0))
        lipb = _box(x0, x1, la, lb, z0 - gap - lip, z0 - gap)                       # lip over the key
        c = c.union(lipb)
    c = c.union(_box(x0 - 1.2, x0, -(w_in / 2 + 1.2), w_in / 2 + 1.2, z0 - gap - lip, z0 + 0.01))   # end stop
    c = c.union(_box(x1 - 2.5, x1 - 1.5, -2.0, 2.0, z0 - 0.2, z0 + 0.01))          # bump
    return c


# BUTTONS: the side switches point along the board's radius, 35 degrees off
# the wall, so each gets a tunnel along its axis (a rib off the inside of the
# face, printable without supports). Values from Waveshare's STEP model:
STEMS = [(-145.0, 19.70), (145.0, 20.21)]   # PWR, BOOT: axis angle from +x, actuator face radius
Z_STEM = -8.30
TUN_W, TUN_H = 2.65, 1.9                    # tunnel; the bar is 2.4 x 1.6 less slim
GUIDE_W = 5.5                               # rib width round the tunnel
GUIDE_GAP = 4.1                             # rib starts this far beyond the actuator
REST_GAP, PRESS = 0.2, 0.45                 # tip-to-actuator at rest; how far a full press pushes it


def _rot(wp, ang):
    return wp.rotate((0, 0, 0), (0, 0, 1), ang)


def plunger(which, mark=False, place=False, slim=0.0):
    """which: 0 = PWR, 1 = BOOT. Built in place (the rest position) and
    returned there if place=True, else laid along +x for printing flat.
    Catch arms spring open past the rib so the switch can't push it out; the
    pad stops on the wall at full press. slim narrows the bar because the
    tunnel prints tighter than drawn (0.2 or 0.4 fits)."""
    ang, r_stem = STEMS[which]
    c, sn = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    r_tip, r_g = r_stem + REST_GAP, r_stem + GUIDE_GAP
    x_r = r_g - 3.2                                       # arm roots
    h, core, w = 0.8 - slim / 2, 0.4, 1.2 - slim / 2
    z0 = Z_STEM - h
    p = _box(r_tip, x_r, -w, w, z0, z0 + 2 * h)
    p = p.union(_box(x_r - 0.01, r_g + 0.01, -core, core, z0, z0 + 2 * h))
    p = p.union(_box(r_g, 50, -w, w, z0, z0 + 2 * h))
    for sy in (-1, 1):
        p = p.union(cq.Workplane("XY").workplane(offset=z0)
                    .polyline([(x_r, sy * core), (x_r, sy * w), (r_g, sy * 1.6), (r_g, sy * 0.9)]).close()
                    .extrude(2 * h))
    p = _rot(p, ang)
    # pad outside the wall, parallel to it
    gx = (REST_GAP + PRESS) * abs(c)
    x_in, x_out = -BODY_W / 2 - gx, -BODY_W / 2 - gx - 1.2
    yc = (-(x_in + x_out) / 2) / abs(c) * sn
    pad = _box(x_out, x_in, yc - 2.4, yc + 2.4, z0, z0 + 2 * h).edges("|Z").fillet(0.5)
    p = p.intersect(_box(x_out, 0, -40, 40, z0 - 1, z0 + 2 * h + 1)).union(pad)
    if mark:
        p = p.cut(_box(x_out - 1, x_out + 0.4, yc - 0.4, yc + 0.4, z0 - 1, z0 + 2 * h + 1))
    return p if place else _rot(p, -ang)


def export(wp, name):
    shape = wp.val()
    cq.exporters.export(cq.Workplane().add(shape), os.path.join(OUT, name), tolerance=0.02, angularTolerance=0.1)
    bb = shape.BoundingBox()
    print(f"{name:20s} {bb.xlen:5.1f} x {bb.ylen:5.1f} x {bb.zlen:5.1f} mm")


if __name__ == "__main__":
    export(shell(), "clawd_shell_angled.stl")
    export(plate(), "clawd_plate.stl")
    export(back(), "clawd_back_flat.stl")
    export(back(key_clip=True), "clawd_back_flat_key.stl")
    export(key(), "clawd_key.stl")
    export(spacer(), "clawd_spacer.stl")
    for slim in (0.2, 0.4):
        export(plunger(0, mark=True, slim=slim), f"clawd_plunger_pwr_angled_slim{round(slim * 10)}.stl")
        export(plunger(1, slim=slim), f"clawd_plunger_boot_angled_slim{round(slim * 10)}.stl")
