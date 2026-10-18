"""Companion prints for the Clawd case: a bed, a desk-edge cable claw, hats.

Run with CadQuery:  python extras.py   -> writes STLs next to this file,
each laid out in its print orientation (no supports needed).

Sizes come from ../buddy_clawd.py, so they follow the case if it changes.
"""
import os
import sys

import cadquery as cq

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import buddy_clawd as case  # noqa: E402
from buddy_clawd import BODY_H, BODY_W, RIM, U, USB_Z, Z_BACK, _box, export  # noqa: E402

case.OUT = os.path.dirname(os.path.abspath(__file__))       # export() writes here

FIT = 0.3                                   # clearance round the case
DEPTH = RIM - Z_BACK                        # case front to back, 21.6
HEAD_TOP = BODY_H / 2                       # top of the head, y = 27


def silhouette(u=U, grow=0.0, h=1.0):
    """Clawd's outline in XY (body, arms, legs) extruded h, grown by `grow`."""
    bw, bh = 10 * u, 9 * u
    arm_y = (case.ARM_Y[0] * u / U, case.ARM_Y[1] * u / U)
    s = _box(-bw / 2 - grow, bw / 2 + grow, -bh / 2 - grow, bh / 2 + grow, 0, h)
    for x0 in (bw / 2, -bw / 2 - u):
        s = s.union(_box(x0 - grow, x0 + u + grow, arm_y[0] - grow, arm_y[1] + grow, 0, h))
    for lx in case.LEGS_X:
        x = lx * u / U
        s = s.union(_box(x - u / 2 - grow, x + u / 2 + grow, -bh / 2 - u - grow, -bh / 2 + 0.01, 0, h))
    return s


def lay_flat(wp, flip=False):
    """Rotate a part built with y up so y points up off the print bed."""
    wp = wp.rotate((0, 0, 0), (1, 0, 0), -90 if flip else 90)
    bb = wp.val().BoundingBox()
    return wp.translate((0, 0, -bb.zmin))


# ---------------------------------------------------------------- bed --
# Clawd lies on its back, face up, in a hollow in the mattress. The USB-C
# plug lies along the middle between the two pillows and the cable drops
# into a notch in the headboard. Bed frame: XY as Clawd's face, z up,
# mattress top at z = 0.

BED_M = 6.0                                  # mattress margin round Clawd
BED_T, BED_HOLLOW = 8.0, 4.0                 # mattress thickness, hollow depth
BED_HEAD = HEAD_TOP + 28                     # headboard line: room for the plug
BLANKET_Y = (-BODY_H / 2 - U - 2, -BODY_H / 2 + 3)   # over the legs, below the screen
PORT_Z = -BED_HOLLOW + (USB_Z - Z_BACK)      # USB-C centre above the mattress top
PEG_X = BODY_W / 2 + U + BED_M / 2 - 1       # blanket pegs, either side of Clawd


def bed():
    x0, x1 = -BODY_W / 2 - U - BED_M, BODY_W / 2 + U + BED_M
    y0 = -BODY_H / 2 - U - BED_M
    b = _box(x0, x1, y0, BED_HEAD, -BED_T, 0).edges("|Z").fillet(4).faces(">Z").edges().fillet(1.5)
    b = b.cut(silhouette(grow=FIT, h=BED_HOLLOW + 1).translate((0, 0, -BED_HOLLOW)))
    # the case's chamfered edges fill its inside corners a little: relieve them
    corners = [(lx + sx * U / 2, -BODY_H / 2) for lx in case.LEGS_X for sx in (-1, 1)]
    corners += [(sx * BODY_W / 2, y) for sx in (-1, 1) for y in case.ARM_Y]
    for cx, cy in corners:
        b = b.cut(_box(cx - 0.8, cx + 0.8, cy - 0.8, cy + 0.8, -BED_HOLLOW, 1))
    # headboard and footboard
    head = _box(x0, x1, BED_HEAD, BED_HEAD + 5, -BED_T, 30).edges("|Y").fillet(4)
    head = head.cut(_box(-3.4, 3.4, BED_HEAD - 1, BED_HEAD + 6, PORT_Z - 2.5, 31))      # cable notch
    foot = _box(x0, x1, y0 - 5, y0, -BED_T, 10).edges("|Y").fillet(3)
    b = b.union(head).union(foot)
    # two pillow halves, a gap down the middle for the plug
    for sx in (-1, 1):
        xa, xb = sorted((sx * 8, sx * 34))
        p = _box(xa, xb, HEAD_TOP + 4, BED_HEAD - 2, 0, 6).edges().fillet(2.4)
        b = b.union(p)
    # holes for the blanket's pegs
    for sx in (-1, 1):
        b = b.cut(cq.Workplane("XY").workplane(offset=-2.6).center(sx * PEG_X, sum(BLANKET_Y) / 2)
                  .circle(1.7).extrude(3))
    return b


def blanket():
    """Arches over Clawd's legs, resting on the mattress either side."""
    face = -BED_HOLLOW + DEPTH                 # Clawd's face above the mattress top
    y0, y1 = BLANKET_Y
    xo = BODY_W / 2 + U + BED_M - 0.5
    xi = BODY_W / 2 + 1.0
    bl = _box(-xo, xo, y0, y1, 0, face + 2.2).cut(_box(-xi, xi, y0 - 1, y1 + 1, -1, face + 0.6))
    bl = bl.edges("|Y and >Z").fillet(1.5)
    roll = cq.Workplane("YZ").workplane(offset=-xo).center(y1, face + 1.25).circle(0.95).extrude(2 * xo)   # turned-down edge, flush with the top
    bl = bl.union(roll)
    for sx in (-1, 1):
        bl = bl.union(cq.Workplane("XY").workplane(offset=-2).center(sx * PEG_X, (y0 + y1) / 2).circle(1.5).extrude(2.01))
    return bl


# -------------------------------------------------------- cable claw --
# A half-size Clawd standing on a clip that hooks over the desk edge. The
# cable snaps into a notch on top of its head. Use frame: x along the edge,
# y up (desk top at y = 0), z towards you (the desk's front face at
# z = -PLATE).

MINI_U = 3.0
PLATE, WALL = 4.0, 2.5
CABLE_D, CABLE_MOUTH = 4.8, 3.6              # seat for a 4-4.5 mm cable, narrower mouth so it snaps in


def cable_claw(desk_t):
    bw, bh = 10 * MINI_U, 9 * MINI_U
    gap = desk_t + 0.4
    feet = WALL                                         # Clawd stands on the clip's top
    fig = silhouette(MINI_U, h=PLATE).translate((0, feet + MINI_U + bh / 2, -PLATE))
    clip = _box(-bw / 2, bw / 2, -gap - WALL, feet + MINI_U, -PLATE, 0)          # front, behind Clawd's legs
    clip = clip.union(_box(-bw / 2, bw / 2, 0, WALL, -PLATE - 16, -PLATE + 0.01))  # over the desk top
    clip = clip.union(_box(-bw / 2, bw / 2, -gap - WALL, -gap, -PLATE - 12, -PLATE + 0.01))   # under it
    c = fig.union(clip)
    top = feet + MINI_U + bh
    seat_y = top - CABLE_D / 2 - 1.2
    c = c.cut(cq.Workplane("XY").workplane(offset=-PLATE - 1).center(0, seat_y).circle(CABLE_D / 2).extrude(PLATE + 2))
    c = c.cut(_box(-CABLE_MOUTH / 2, CABLE_MOUTH / 2, seat_y, top + 1, -PLATE - 1, 1))
    # eyes, sunk into the front
    ey = feet + MINI_U + bh / 2 + 3
    for sx in (-1, 1):
        c = c.cut(_box(sx * 5 - 1.4, sx * 5 + 1.4, ey - 2.2, ey + 2.2, -0.8, 1))
    return c


def claw_print(desk_t):
    """Front face down on the bed."""
    c = cable_claw(desk_t).rotate((0, 0, 0), (0, 1, 0), 180)
    return c.translate((0, 0, -c.val().BoundingBox().zmin))


# --------------------------------------------------------------- hats --
# Small hats that plug into the USB-C opening on top of the head: a peg the
# shape of the opening grips its walls and stops short of the connector.
# Take the hat off to charge. Clawd frame, hats centred on the port.

PORT_W, PORT_H, PORT_R = 12.6, 7.2, 2.5      # the case's USB-C opening (x, z, corner radius)
PORT_FLOOR = 23.0                            # the connector's face, below the opening
PEG_FIT = 0.15                               # clearance per side; less if hats come off too easily


def peg():
    w, h = PORT_W - 2 * PEG_FIT, PORT_H - 2 * PEG_FIT
    p = (_box(-w / 2, w / 2, PORT_FLOOR + 0.4, HEAD_TOP + 0.01, USB_Z - h / 2, USB_Z + h / 2)
         .edges("|Y").fillet(PORT_R - PEG_FIT).faces("<Y").chamfer(0.4))
    # a ridge front and back, PEG_FIT + 0.1 proud: the slit lets the halves
    # squeeze past and they spring back against the walls
    for sz in (-1, 1):
        z0 = USB_Z + sz * h / 2
        p = p.union(_box(-4, 4, 24.6, 25.6, min(z0, z0 + sz * (PEG_FIT + 0.1)), max(z0, z0 + sz * (PEG_FIT + 0.1))))
    return p.cut(_box(-w, w, PORT_FLOOR, HEAD_TOP - 1, USB_Z - 0.5, USB_Z + 0.5))   # slit, so it springs


def _plate(y, rx, rz):
    """An ellipse in the XZ plane at height y; extrude(-h) goes up."""
    return cq.Workplane("XZ", origin=(0, y, 0)).center(0, USB_Z).ellipse(rx, rz)


def _ball(r, y, z=USB_Z):
    return cq.Workplane("XY").sphere(r).translate((0, y, z))


def hat_party():
    cone = _plate(HEAD_TOP, 7.6, 4.7).workplane(offset=-22).ellipse(0.9, 0.9).loft()
    stripe = _plate(HEAD_TOP + 3, 6.9, 4.3).extrude(-2)
    return peg().union(cone).union(stripe).union(_ball(2.2, HEAD_TOP + 22.5))


def hat_beanie():
    cuff = _box(-7.6, 7.6, HEAD_TOP, HEAD_TOP + 3, USB_Z - 4.8, USB_Z + 4.8).edges("|Y").fillet(2.4)
    secs = _plate(HEAD_TOP + 3, 7.2, 4.4)
    for i in range(1, 6):
        k = (1 - (i / 6) ** 2) ** 0.5
        secs = secs.workplane(offset=-8 / 6).ellipse(max(7.2 * k, 1.2), max(4.4 * k, 1.0))
    dome = secs.loft(ruled=False)
    return peg().union(cuff).union(dome).union(_ball(2.6, HEAD_TOP + 3 + 8 + 1.2))


def hat_crown():
    w, h, wall, tall = PORT_W + 1.6, PORT_H + 1.6, 0.9, 8.0
    base = _box(-w / 2, w / 2, HEAD_TOP, HEAD_TOP + 1.2, USB_Z - h / 2, USB_Z + h / 2).edges("|Y").fillet(PORT_R + 0.8)
    ring = _box(-w / 2, w / 2, HEAD_TOP, HEAD_TOP + tall, USB_Z - h / 2, USB_Z + h / 2).edges("|Y").fillet(PORT_R + 0.8)
    ring = ring.cut(_box(-w / 2 + wall, w / 2 - wall, HEAD_TOP + 1.2, HEAD_TOP + tall + 1, USB_Z - h / 2 + wall, USB_Z + h / 2 - wall)
                    .edges("|Y").fillet(PORT_R))
    for x in (-3.6, 0, 3.6):                                   # zigzag points, front and back
        ring = ring.cut(cq.Workplane("XY", origin=(0, 0, USB_Z - h)).polyline(
            [(x - 1.8, HEAD_TOP + tall + 0.1), (x + 1.8, HEAD_TOP + tall + 0.1), (x, HEAD_TOP + tall - 3.2)]).close().extrude(2 * h))
    for sx in (-1, 1):                                         # and the ends
        ring = ring.cut(cq.Workplane("YZ", origin=(sx * w / 2 - sx * 2, 0, 0)).polyline(
            [(HEAD_TOP + tall + 0.1, USB_Z - 1.8), (HEAD_TOP + tall + 0.1, USB_Z + 1.8), (HEAD_TOP + tall - 3.2, USB_Z)]).close().extrude(4 * sx))
    return peg().union(base).union(ring)


TOPHAT_BRIM = (13.0, 8.2, 1.2)               # brim rx, rz, thickness
TOPHAT_CROWN = (6.8, 4.6, 15.0)              # crown rx, rz, height


def hat_top():
    """The top hat's crown and peg; the brim is a separate ring."""
    rx, rz, h = TOPHAT_CROWN
    crown = _plate(HEAD_TOP + TOPHAT_BRIM[2], rx, rz).extrude(-h)
    band = _plate(HEAD_TOP + TOPHAT_BRIM[2], rx + 0.3, rz + 0.3).extrude(-2.6)
    foot = _plate(HEAD_TOP, rx, rz).extrude(-TOPHAT_BRIM[2] - 0.01)   # inside the brim
    return peg().union(foot).union(crown).union(band)


def hat_top_brim():
    """A flat ring: slide it over the peg before plugging the hat in."""
    rx, rz, t = TOPHAT_BRIM
    ring = _plate(HEAD_TOP, rx, rz).extrude(-t)
    return ring.cut(_plate(HEAD_TOP - 1, TOPHAT_CROWN[0] + 0.25, TOPHAT_CROWN[1] + 0.25).extrude(-t - 2))


def hat_gradcap():
    skull = _box(-7.4, 7.4, HEAD_TOP, HEAD_TOP + 5, USB_Z - 4.6, USB_Z + 4.6).edges("|Y").fillet(2.4)
    board = _box(-14, 14, HEAD_TOP + 5, HEAD_TOP + 6.6, USB_Z - 14, USB_Z + 14).edges("|Y").fillet(0.8)
    board = board.rotate((0, 0, USB_Z), (0, 1, USB_Z), 45)
    button = cq.Workplane("XZ", origin=(0, HEAD_TOP + 6.6, 0)).center(0, USB_Z).circle(1.6).extrude(-0.8)
    tassel = cq.Workplane("XZ", origin=(0, HEAD_TOP + 1.2, 0)).center(15, USB_Z).circle(0.9).extrude(-3.81)   # hangs clear of the head
    return peg().union(skull).union(board).union(button).union(tassel)


if __name__ == "__main__":
    export(bed().translate((0, 0, BED_T)), "clawd_bed.stl")
    bl = blanket().rotate((0, 0, 0), (0, 1, 0), 180)                     # top down on the print bed
    export(bl.translate((0, 0, -bl.val().BoundingBox().zmin)), "clawd_bed_blanket.stl")
    for t in (18, 25):
        export(claw_print(t), f"clawd_cable_claw_{t}mm.stl")
    export(lay_flat(hat_party()), "clawd_hat_party.stl")
    export(lay_flat(hat_beanie()), "clawd_hat_beanie.stl")
    export(lay_flat(hat_crown()), "clawd_hat_crown.stl")
    export(lay_flat(hat_gradcap(), flip=True), "clawd_hat_gradcap.stl")
    export(lay_flat(hat_top(), flip=True), "clawd_hat_top.stl")
    export(lay_flat(hat_top_brim()), "clawd_hat_top_brim.stl")
