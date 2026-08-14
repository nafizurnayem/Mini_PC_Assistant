# -*- coding: utf-8 -*-
"""
Pico Assistant - tilting L-base case generator for Fusion 360
=============================================================

Builds a complete, printable enclosure for:

  * 3.2" ILI9341 SPI TFT, 240x320, portrait   (geometry taken from 320x240.step)
  * ESP32-S3 Super Mini
  * TTP223 capacitive touch module (senses through the front wall)
  * EC11 rotary encoder (volume), shaft up through the top wall
  * 12 mm magnetic buzzer behind a side grille

Output components
  1  FrontShell     bezel, display seat, touch window, insert bosses
  2  BackShell      ESP32 rails, encoder mount, buzzer grille, hinge bosses
  3  Base           L-shaped foot with twin cheeks, 90 deg .. 45 deg tilt
  4  Knob           EC11 knob, D-shaft
  5  Thumbwheel     tool-free tilt lock (print 2)
  6  _Mock*         display / ESP32 dummies for visual fit check (deletable)

Everything below MM_PARAMS is editable - change a number, rerun the script.
Fusion's internal unit is cm, so every literal is passed through mm().

Run:  Utilities tab -> ADD-INS -> Scripts and Add-Ins -> Scripts -> + ->
      pick this folder -> Run
"""

import adsk.core
import adsk.fusion
import traceback
import math

# ----------------------------------------------------------------------------
#  PARAMETERS  (all millimetres, all degrees)
# ----------------------------------------------------------------------------

BUILD_MOCKS = True        # set False to skip the display / ESP32 dummy solids
BUILD_HARDWARE = True     # knob + thumbwheel

# --- display module, measured from the supplied STEP -------------------------
DISP_PCB_W   = 55.04      # PCB width   (case X)
DISP_PCB_H   = 89.30      # PCB height  (case Y, portrait)
DISP_PCB_T   = 1.60
DISP_LCD_W   = 55.04      # glass/metal body, full PCB width
DISP_LCD_H   = 77.70      # centred on the PCB
DISP_LCD_T   = 2.30       # body height above the PCB top face
ACT_W        = 48.60      # active area  (240 px)
ACT_H        = 64.80      # active area  (320 px)
ACT_OFF      = 2.95       # active-area centre sits this far ABOVE the PCB centre
HDR_BACK     = 8.60       # header pins protrude this far behind the PCB
HDR14_W      = 35.56      # 14-pin header, bottom edge
HDR14_OFF    = 1.27       # its centre, measured from the PCB bottom edge
HDR4_W       = 10.16      # 4-pin header, top edge
HDR4_OFF     = 3.00       # its centre, measured from the PCB top edge

# --- shell ------------------------------------------------------------------
FIT          = 0.40       # per-side clearance around the PCB
WALL         = 2.40
FRONT_WALL   = 2.50
BACK_WALL    = 2.50
CHIN         = 26.00      # solid front face below the PCB (touch pad lives here)
BROW         = 16.00      # solid front face above the PCB
CORNER_R     = 4.00
GLASS_GAP    = 0.35       # air gap in front of the LCD glass

REAR_CLEAR   = 12.75      # cavity depth behind the PCB back face
Z_SPLIT      = -8.00      # front/back shell parting plane
LIP_H        = 2.00       # back-shell lip height
LIP_T        = 1.20       # back-shell lip wall thickness
LIP_CLR      = 0.15       # per-side lip clearance

# --- fasteners --------------------------------------------------------------
INSERT_D     = 4.00       # drill for a standard M3 brass heat-set insert
INSERT_L     = 5.40
BOSS_D       = 7.50
SCREW_CLR    = 3.40       # M3 clearance
CSK_D        = 6.20       # M3 countersunk head
CSK_DEPTH    = 1.60
BOSS_X       = 22.00
BOSS_Y_BOT   = 8.00

# --- touch pad --------------------------------------------------------------
TOUCH_Y      = 14.00
TOUCH_FACE_D = 24.00      # cosmetic dish on the outside
TOUCH_FACE_Z = 0.40
TOUCH_MEM_D  = 22.00      # thinned sensing membrane
TOUCH_MEM_T  = 1.20
TTP_W        = 15.30      # TTP223 breakout footprint - MEASURE YOURS
TTP_H        = 11.30
TTP_RIB      = 1.60
TTP_RIB_H    = 4.00

# --- EC11 rotary encoder, shaft up through the top wall ---------------------
EC11_X       = 11.00
EC11_Z       = -13.00
EC11_BUSH_D  = 7.40       # M7 bushing clearance
EC11_BODY    = 12.40      # square body clearance
EC11_BODY_H  = 6.80       # body height below the panel
EC11_PIN_HOLE = False     # True -> adds the anti-rotation pin hole
EC11_PIN_OFF = 6.50

# --- buzzer -----------------------------------------------------------------
# Sits flat on the inner back wall up in the brow, where the full cavity depth
# is available; the grille is on the left side wall alongside it.
BUZ_X        = -8.00
BUZ_Y        = 121.50
BUZ_D        = 12.40      # buzzer body diameter
BUZ_RING_T   = 1.00       # cradle wall
BUZ_RING_L   = 4.00       # cradle depth
BUZ_BODY_H   = 9.50
GRILLE_Z     = -13.00
GRILLE_D     = 2.00
GRILLE_P     = 4.00       # pitch
GRILLE_N     = 3          # 3 x 3

# --- ESP32-S3 Super Mini ----------------------------------------------------
ESP_W        = 18.00
ESP_L        = 22.50
ESP_T        = 1.20
ESP_Y0       = 1.50       # bottom edge of the board (USB-C end)
ESP_STANDOFF = 3.00       # board underside above the back-shell inner face
RAIL_T       = 2.80       # rail thickness outboard of the board edge
RAIL_GRIP    = 0.80       # how far the groove overlaps the board edge
RAIL_SLOT    = 1.60       # groove height
USB_W        = 12.00
USB_H        = 4.70
USB_R        = 1.20

# --- PCB press posts (back shell pushes the display onto its ledge) ---------
POST_D       = 6.00
POST_X       = 24.00
POST_GAP     = 0.10       # stop just short of the PCB; add foam tape at assembly

# --- tilt hinge -------------------------------------------------------------
PIVOT_Y      = 38.00      # on the enclosure side wall
LOCK_R       = 17.00      # lock screw radius about the pivot
TILT_MIN     = 45.0       # most reclined
TILT_MAX     = 90.0       # upright
HINGE_BOSS_D = 11.00
HINGE_PROUD  = 4.00       # bearing pads stand this far off the wall
HINGE_PAD    = 2.00       # connecting web
CHEEK_T      = 5.00
CHEEK_CLR    = 0.30       # per-side clearance to the bearing pads
SLOT_DETENT  = 3.50       # M3 seats here
SLOT_NECK    = 3.05       # soft detent - screw passes with light force
DETENT_STEP  = 15.0

# --- base -------------------------------------------------------------------
PLATE_T      = 6.00
PIVOT_H      = 43.00      # plate top face -> pivot axis
PLATE_X      = 41.00      # half width
PLATE_ZF     = 26.00      # front edge
PLATE_ZR     = -56.00     # rear edge
CHEEK_TOP    = 52.00      # above the plate top face
POCKET_D     = 0.00       # underside lightening pocket; 0 = none.
                          # A pocket in the bottom face would print as a large
                          # unsupported ceiling, so leave it off and let the
                          # slicer hollow the plate with infill instead.
FOOT_D       = 10.00
FOOT_DEPTH   = 1.00
CABLE_W      = 14.00
CABLE_L      = 12.00

# --- knob / thumbwheel ------------------------------------------------------
KNOB_D       = 26.00
KNOB_H       = 14.00
KNOB_BORE    = 6.15
KNOB_FLAT    = 4.60       # D-shaft flat, measured across
KNOB_BORE_L  = 11.00
KNOB_SCALLOP = 20
TW_D         = 22.00
TW_H         = 9.00
TW_HEX       = 5.60       # M3 hex head across flats
TW_HEX_D     = 2.40
TW_FLANGE_D  = 12.00
TW_FLANGE_H  = 1.50

# ----------------------------------------------------------------------------
#  DERIVED
# ----------------------------------------------------------------------------

CASE_W   = DISP_PCB_W + 2 * FIT + 2 * WALL
CASE_H   = CHIN + DISP_PCB_H + BROW
IN_W     = CASE_W - 2 * WALL                     # cavity width  (== PCB + 2*FIT)
IN_H     = CASE_H - 2 * WALL
IN_CY    = CASE_H / 2.0

PCB_Y0   = CHIN
PCB_Y1   = CHIN + DISP_PCB_H
PCB_CY   = (PCB_Y0 + PCB_Y1) / 2.0
LCD_CY   = PCB_CY
WIN_CY   = PCB_CY + ACT_OFF

Z_WALL_IN = -FRONT_WALL
Z_LCD_TOP = Z_WALL_IN - GLASS_GAP
Z_LEDGE   = Z_LCD_TOP - DISP_LCD_T               # PCB front face rests here
Z_PCB_BOT = Z_LEDGE - DISP_PCB_T
Z_BACK_IN = Z_PCB_BOT - REAR_CLEAR
Z_BACK_OUT = Z_BACK_IN - BACK_WALL
CASE_D    = -Z_BACK_OUT

BOSS_Y_TOP = CASE_H - BOSS_Y_BOT
PIVOT_Z    = Z_BACK_OUT / 2.0                    # mid-depth
LOCK_Y     = PIVOT_Y - LOCK_R
LOCK_Z     = PIVOT_Z

BOSS_XY = [(sx * BOSS_X, by) for sx in (-1, 1) for by in (BOSS_Y_BOT, BOSS_Y_TOP)]

WALL_X   = CASE_W / 2.0                          # outer face of the side wall
IN_X     = IN_W / 2.0                            # inner face of the side wall
PAD_X    = WALL_X + HINGE_PROUD                  # cheek bearing face

ESP_Z0   = Z_BACK_IN + ESP_STANDOFF              # board underside
ESP_Z1   = ESP_Z0 + ESP_T
ESP_Y1   = ESP_Y0 + ESP_L

# ----------------------------------------------------------------------------
#  FUSION PLUMBING
# ----------------------------------------------------------------------------

app = None
ui = None
des = None
root = None
tbm = None


def mm(v):
    return v / 10.0


def P(x, y, z):
    return adsk.core.Point3D.create(mm(x), mm(y), mm(z))


def V(x, y, z):
    return adsk.core.Vector3D.create(x, y, z)


def box(x0, y0, z0, x1, y1, z1):
    """Axis-aligned box from two opposite corners, mm."""
    cx, cy, cz = (x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0
    L, W, H = abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)
    obb = adsk.core.OrientedBoundingBox3D.create(
        P(cx, cy, cz), V(1, 0, 0), V(0, 1, 0), mm(L), mm(W), mm(H))
    return tbm.createBox(obb)


def cyl(p0, p1, r):
    return tbm.createCylinderOrCone(P(*p0), mm(r), P(*p1), mm(r))


def cone(p0, r0, p1, r1):
    return tbm.createCylinderOrCone(P(*p0), mm(r0), P(*p1), mm(r1))


def uni(a, b):
    tbm.booleanOperation(a, b, adsk.fusion.BooleanTypes.UnionBooleanType)
    return a


def sub(a, b):
    tbm.booleanOperation(a, b, adsk.fusion.BooleanTypes.DifferenceBooleanType)
    return a


def isect(a, b):
    tbm.booleanOperation(a, b, adsk.fusion.BooleanTypes.IntersectionBooleanType)
    return a


# --- rounded rectangular prisms, one per extrusion axis ---------------------

def rr_z(cx, cy, w, h, r, z0, z1):
    r = max(0.0, min(r, w / 2.0 - 0.001, h / 2.0 - 0.001))
    if r <= 0.005:
        return box(cx - w / 2, cy - h / 2, z0, cx + w / 2, cy + h / 2, z1)
    b = box(cx - w / 2, cy - h / 2 + r, z0, cx + w / 2, cy + h / 2 - r, z1)
    uni(b, box(cx - w / 2 + r, cy - h / 2, z0, cx + w / 2 - r, cy + h / 2, z1))
    for sx in (-1, 1):
        for sy in (-1, 1):
            px, py = cx + sx * (w / 2 - r), cy + sy * (h / 2 - r)
            uni(b, cyl((px, py, z0), (px, py, z1), r))
    return b


def rr_x(cy, cz, h, d, r, x0, x1):
    r = max(0.0, min(r, h / 2.0 - 0.001, d / 2.0 - 0.001))
    if r <= 0.005:
        return box(x0, cy - h / 2, cz - d / 2, x1, cy + h / 2, cz + d / 2)
    b = box(x0, cy - h / 2 + r, cz - d / 2, x1, cy + h / 2 - r, cz + d / 2)
    uni(b, box(x0, cy - h / 2, cz - d / 2 + r, x1, cy + h / 2, cz + d / 2 - r))
    for sy in (-1, 1):
        for sz in (-1, 1):
            py, pz = cy + sy * (h / 2 - r), cz + sz * (d / 2 - r)
            uni(b, cyl((x0, py, pz), (x1, py, pz), r))
    return b


def rr_y(cx, cz, w, d, r, y0, y1):
    r = max(0.0, min(r, w / 2.0 - 0.001, d / 2.0 - 0.001))
    if r <= 0.005:
        return box(cx - w / 2, y0, cz - d / 2, cx + w / 2, y1, cz + d / 2)
    b = box(cx - w / 2, y0, cz - d / 2 + r, cx + w / 2, y1, cz + d / 2 - r)
    uni(b, box(cx - w / 2 + r, y0, cz - d / 2, cx + w / 2 - r, y1, cz + d / 2))
    for sx in (-1, 1):
        for sz in (-1, 1):
            px, pz = cx + sx * (w / 2 - r), cz + sz * (d / 2 - r)
            uni(b, cyl((px, y0, pz), (px, y1, pz), r))
    return b


def stadium_x(y0, y1, cz, width, x0, x1):
    """Capsule running along Y, extruded along X."""
    r = width / 2.0
    b = box(x0, y0, cz - r, x1, y1, cz + r)
    uni(b, cyl((x0, y0, cz), (x1, y0, cz), r))
    uni(b, cyl((x0, y1, cz), (x1, y1, cz), r))
    return b


def hex_prism_y(cx, cz, across_flats, y0, y1):
    """Hexagonal prism about the Y axis, built as the intersection of 3 slabs."""
    a = across_flats / 2.0
    big = across_flats * 3.0
    body = None
    for k in range(3):
        ang = math.radians(60.0 * k)
        slab = box(cx - big, y0, cz - a, cx + big, y1, cz + a)
        m = adsk.core.Matrix3D.create()
        m.setToRotation(ang, V(0, 1, 0), P(cx, (y0 + y1) / 2.0, cz))
        tbm.transform(slab, m)
        body = slab if body is None else isect(body, slab)
    return body


def new_comp(name, dx=0.0, dy=0.0, dz=0.0):
    m = adsk.core.Matrix3D.create()
    m.translation = adsk.core.Vector3D.create(mm(dx), mm(dy), mm(dz))
    occ = root.occurrences.addNewComponent(m)
    occ.component.name = name
    return occ.component


def add_body(comp, body, name):
    if des.designType == adsk.fusion.DesignTypes.ParametricDesignType:
        bf = comp.features.baseFeatures.add()
        bf.startEdit()
        b = comp.bRepBodies.add(body, bf)
        bf.finishEdit()
    else:
        b = comp.bRepBodies.add(body)
    try:
        b.name = name
    except Exception:
        pass
    return b


# ----------------------------------------------------------------------------
#  1.  FRONT SHELL
# ----------------------------------------------------------------------------

def build_front_shell():
    s = rr_z(0, IN_CY, CASE_W, CASE_H, CORNER_R, 0.0, Z_SPLIT)

    # -- cavity behind the display seat, full inner rectangle
    sub(s, rr_z(0, IN_CY, IN_W, IN_H, CORNER_R - WALL, Z_LEDGE, Z_SPLIT - 0.001))

    # -- chin and brow cavities run all the way up to the front wall
    chin_h = CHIN - WALL
    sub(s, rr_z(0, (WALL + CHIN) / 2.0, IN_W, chin_h, 1.5, Z_WALL_IN, Z_LEDGE + 0.001))
    brow_h = CASE_H - WALL - PCB_Y1
    sub(s, rr_z(0, (PCB_Y1 + CASE_H - WALL) / 2.0, IN_W, brow_h, 1.5,
                Z_WALL_IN, Z_LEDGE + 0.001))

    # -- LCD body pocket, leaves a 5.8 mm ledge top and bottom for the PCB face
    sub(s, rr_z(0, LCD_CY, DISP_LCD_W + 2 * FIT, DISP_LCD_H + 2 * FIT, 1.5,
                Z_WALL_IN, Z_LEDGE))

    # -- viewing window, then the shallow bezel step on the outside
    sub(s, rr_z(0, WIN_CY, ACT_W + 1.2, ACT_H + 1.2, 1.5, 0.5, Z_WALL_IN - 0.5))
    sub(s, rr_z(0, WIN_CY, ACT_W + 3.2, ACT_H + 3.2, 2.5, 0.5, -0.80))

    # -- touch pad: cosmetic dish outside, thinned sensing membrane inside
    sub(s, cyl((0, TOUCH_Y, 0.5), (0, TOUCH_Y, -TOUCH_FACE_Z), TOUCH_FACE_D / 2.0))
    sub(s, cyl((0, TOUCH_Y, Z_WALL_IN - 0.01),
               (0, TOUCH_Y, -(TOUCH_FACE_Z + TOUCH_MEM_T)), TOUCH_MEM_D / 2.0))

    # -- back-shell lip rebate (before any boss is added)
    sub(s, rr_z(0, IN_CY, CASE_W - 2 * LIP_T, CASE_H - 2 * LIP_T, CORNER_R - LIP_T,
                Z_SPLIT + LIP_H, Z_SPLIT - 0.001))

    # -- insert bosses + gussets
    zb = Z_SPLIT
    for (bx, by) in BOSS_XY:
        uni(s, cyl((bx, by, Z_WALL_IN), (bx, by, zb), BOSS_D / 2.0))
        gy0, gy1 = (WALL, by) if by < IN_CY else (by, CASE_H - WALL)
        uni(s, box(bx - 2.0, gy0, Z_WALL_IN, bx + 2.0, gy1, zb))
    for (bx, by) in BOSS_XY:
        sub(s, cyl((bx, by, zb + 0.01), (bx, by, zb - INSERT_L), INSERT_D / 2.0))

    # -- TTP223 locating ribs
    rz0 = -(TOUCH_FACE_Z + TOUCH_MEM_T)
    rz1 = rz0 - TTP_RIB_H
    hw, hh = (TTP_W + 0.4) / 2.0, (TTP_H + 0.4) / 2.0
    uni(s, box(-3.0, TOUCH_Y + hh, rz0, 3.0, TOUCH_Y + hh + TTP_RIB, rz1))
    uni(s, box(-3.0, TOUCH_Y - hh - TTP_RIB, rz0, 3.0, TOUCH_Y - hh, rz1))
    uni(s, box(-hw - TTP_RIB, TOUCH_Y - 3.0, rz0, -hw, TOUCH_Y + 3.0, rz1))
    uni(s, box(hw, TOUCH_Y - 3.0, rz0, hw + TTP_RIB, TOUCH_Y + 3.0, rz1))
    return s


# ----------------------------------------------------------------------------
#  2.  BACK SHELL
# ----------------------------------------------------------------------------

def build_back_shell():
    s = rr_z(0, IN_CY, CASE_W, CASE_H, CORNER_R, Z_SPLIT, Z_BACK_OUT)

    # -- lip that plugs into the front-shell rebate
    lip = rr_z(0, IN_CY, CASE_W - 2 * LIP_T - 2 * LIP_CLR,
               CASE_H - 2 * LIP_T - 2 * LIP_CLR, CORNER_R - LIP_T,
               Z_SPLIT, Z_SPLIT + LIP_H)
    sub(lip, rr_z(0, IN_CY, IN_W, IN_H, CORNER_R - WALL, Z_SPLIT + 0.01,
                  Z_SPLIT + LIP_H + 0.01))
    uni(s, lip)

    # -- cavity
    sub(s, rr_z(0, IN_CY, IN_W, IN_H, CORNER_R - WALL, Z_SPLIT + 0.01, Z_BACK_IN))

    # -- hinge bearing pads + web, both side walls
    for sx in (-1, 1):
        x0, x1 = sx * WALL_X, sx * (WALL_X + HINGE_PAD)
        uni(s, stadium_x(LOCK_Y, PIVOT_Y, PIVOT_Z, HINGE_BOSS_D,
                         min(x0, x1), max(x0, x1)))
        for py in (PIVOT_Y, LOCK_Y):
            a = (sx * WALL_X, py, PIVOT_Z)
            b = (sx * PAD_X, py, PIVOT_Z)
            uni(s, cyl(a, b, HINGE_BOSS_D / 2.0))
    for sx in (-1, 1):
        for py in (PIVOT_Y, LOCK_Y):
            a = (sx * (PAD_X + 0.01), py, PIVOT_Z)
            b = (sx * (PAD_X - INSERT_L), py, PIVOT_Z)
            sub(s, cyl(a, b, INSERT_D / 2.0))

    # -- screw columns, through holes and countersinks
    for (bx, by) in BOSS_XY:
        uni(s, cyl((bx, by, Z_BACK_IN), (bx, by, Z_SPLIT), BOSS_D / 2.0))
    for (bx, by) in BOSS_XY:
        sub(s, cyl((bx, by, Z_BACK_OUT - 0.5), (bx, by, Z_SPLIT + 0.5),
                   SCREW_CLR / 2.0))
        sub(s, cone((bx, by, Z_BACK_OUT - 0.05), CSK_D / 2.0,
                    (bx, by, Z_BACK_OUT + CSK_DEPTH), SCREW_CLR / 2.0))

    # -- press posts that hold the display against its ledge
    for sx in (-1, 1):
        for py in (PCB_Y0 + 4.0, PCB_Y1 - 4.0):
            uni(s, cyl((sx * POST_X, py, Z_BACK_IN),
                       (sx * POST_X, py, Z_PCB_BOT - POST_GAP), POST_D / 2.0))

    # -- ESP32-S3 Super Mini card rails
    ri = ESP_W / 2.0 - RAIL_GRIP
    ro = ri + RAIL_T
    gz0 = ESP_Z0 - 0.30
    gz1 = gz0 + RAIL_SLOT
    for sx in (-1, 1):
        rail = box(sx * ri, ESP_Y0 - 1.0, Z_BACK_IN, sx * ro, ESP_Y1 + 1.5,
                   ESP_Z1 + 2.0)
        sub(rail, box(sx * (ri - 0.01), ESP_Y0 - 1.5, gz0, sx * (ro + 0.01),
                      ESP_Y1 + 2.0, gz1))
        uni(s, rail)
    uni(s, box(-ri, ESP_Y0 - 1.0, Z_BACK_IN, ri, ESP_Y0, ESP_Z0 - 0.30))

    # -- USB-C opening in the bottom wall
    usb_cz = ESP_Z1 + USB_H / 2.0 - 0.2
    sub(s, rr_y(0, usb_cz, USB_W, USB_H, USB_R, -1.0, WALL + 1.0))

    # -- EC11 encoder through the top wall
    ty_in = CASE_H - WALL
    sub(s, cyl((EC11_X, CASE_H + 1.0, EC11_Z), (EC11_X, ty_in - 1.0, EC11_Z),
               EC11_BUSH_D / 2.0))
    if EC11_PIN_HOLE:
        sub(s, cyl((EC11_X + EC11_PIN_OFF, CASE_H + 1.0, EC11_Z),
                   (EC11_X + EC11_PIN_OFF, ty_in - 1.0, EC11_Z), 1.30))

    # -- buzzer cradle on the inner back wall (brow), clipped to the cavity
    ring = cyl((BUZ_X, BUZ_Y, Z_BACK_IN), (BUZ_X, BUZ_Y, Z_BACK_IN + BUZ_RING_L),
               BUZ_D / 2.0 + BUZ_RING_T)
    sub(ring, cyl((BUZ_X, BUZ_Y, Z_BACK_IN - 0.5),
                  (BUZ_X, BUZ_Y, Z_BACK_IN + BUZ_RING_L + 0.5), BUZ_D / 2.0))
    isect(ring, box(-IN_X, WALL, Z_BACK_IN - 1.0, IN_X, CASE_H - WALL,
                    Z_BACK_IN + BUZ_RING_L + 1.0))
    uni(s, ring)

    # -- side grille (left wall, alongside the buzzer)
    k = (GRILLE_N - 1) / 2.0
    for i in range(GRILLE_N):
        for j in range(GRILLE_N):
            gy = BUZ_Y + (i - k) * GRILLE_P
            gz = GRILLE_Z + (j - k) * GRILLE_P
            sub(s, cyl((-IN_X + 1.0, gy, gz), (-WALL_X - 1.0, gy, gz),
                       GRILLE_D / 2.0))
    return s


# ----------------------------------------------------------------------------
#  3.  BASE
# ----------------------------------------------------------------------------

def lock_point(theta_deg):
    """Lock-screw centre in base coords for a given tilt, 90 deg = upright."""
    t = math.radians(TILT_MAX - theta_deg)          # 0 at upright
    py = PLATE_T + PIVOT_H
    return (py - LOCK_R * math.cos(t), PIVOT_Z - LOCK_R * math.sin(t))


def build_base():
    py = PLATE_T + PIVOT_H

    # -- plate
    b = rr_y(0, (PLATE_ZF + PLATE_ZR) / 2.0, 2 * PLATE_X, PLATE_ZF - PLATE_ZR,
             8.0, 0.0, PLATE_T)

    # -- cheeks
    cx0 = PAD_X + CHEEK_CLR
    cx1 = cx0 + CHEEK_T
    for sx in (-1, 1):
        a, c = sx * cx0, sx * cx1
        lo, hi = min(a, c), max(a, c)
        # slim upper fin carrying the pivot and the lock arc
        fin = rr_x((PLATE_T + CHEEK_TOP) / 2.0, PIVOT_Z + 2.0,
                   PLATE_T + CHEEK_TOP, 38.0, 8.0, lo, hi)
        # broader foot blending into the plate
        uni(fin, rr_x(PLATE_T + 8.0, PIVOT_Z - 2.0, 2 * (PLATE_T + 8.0), 52.0,
                      6.0, lo, hi))
        uni(fin, cyl((a, py, PIVOT_Z), (c, py, PIVOT_Z), HINGE_BOSS_D / 2.0 + 3.5))
        isect(fin, box(lo - 1, 0.0, PLATE_ZF, hi + 1,
                       CHEEK_TOP + PLATE_T, PLATE_ZR))
        uni(b, fin)

    # -- pivot hole
    for sx in (-1, 1):
        sub(b, cyl((sx * (cx0 - 1.0), py, PIVOT_Z), (sx * (cx1 + 1.0), py, PIVOT_Z),
                   SCREW_CLR / 2.0))

    # -- soft-detent arc slot
    n_det = int(round((TILT_MAX - TILT_MIN) / DETENT_STEP))
    for sx in (-1, 1):
        xa, xb = sx * (cx0 - 1.0), sx * (cx1 + 1.0)
        steps = int(round(TILT_MAX - TILT_MIN))
        for i in range(steps + 1):
            ly, lz = lock_point(TILT_MAX - i)
            sub(b, cyl((xa, ly, lz), (xb, ly, lz), SLOT_NECK / 2.0))
        for d in range(n_det + 1):
            ly, lz = lock_point(TILT_MAX - d * DETENT_STEP)
            sub(b, cyl((xa, ly, lz), (xb, ly, lz), SLOT_DETENT / 2.0))

    # -- optional underside lightening pocket
    if POCKET_D > 0.01:
        sub(b, rr_y(0, (PLATE_ZF - 6 + PLATE_ZR + 6) / 2.0, 60.0,
                    (PLATE_ZF - 6) - (PLATE_ZR + 6), 6.0, -0.5, POCKET_D))

    # -- cable pass-through
    sub(b, rr_y(0, PLATE_ZR + 22.0, CABLE_W, CABLE_L, 6.0, -1.0, PLATE_T + 1.0))

    # -- silicone foot recesses
    for sx in (-1, 1):
        for fz in (PLATE_ZF - 12.0, PLATE_ZR + 12.0):
            sub(b, cyl((sx * 35.0, -0.5, fz), (sx * 35.0, FOOT_DEPTH, fz),
                       FOOT_D / 2.0))
    return b


# ----------------------------------------------------------------------------
#  4 / 5.  KNOB AND THUMBWHEEL
# ----------------------------------------------------------------------------

def build_knob():
    k = cyl((0, 0, 0), (0, KNOB_H, 0), KNOB_D / 2.0)
    for i in range(KNOB_SCALLOP):
        a = 2 * math.pi * i / KNOB_SCALLOP
        px, pz = (KNOB_D / 2.0) * math.cos(a), (KNOB_D / 2.0) * math.sin(a)
        sub(k, cyl((px, -0.5, pz), (px, KNOB_H - 2.0, pz), 1.60))
    sub(k, cone((0, KNOB_H + 0.01, 0), KNOB_D / 2.0 - 1.0,
                (0, KNOB_H - 1.2, 0), KNOB_D / 2.0 - 3.0))
    bore = cyl((0, -0.5, 0), (0, KNOB_BORE_L, 0), KNOB_BORE / 2.0)
    sub(bore, box(-KNOB_BORE, -1.0, KNOB_FLAT - KNOB_BORE / 2.0,
                  KNOB_BORE, KNOB_BORE_L + 0.5, KNOB_BORE))
    sub(k, bore)
    return k


def build_thumbwheel():
    t = cyl((0, 0, 0), (0, TW_H, 0), TW_D / 2.0)
    uni(t, cyl((0, -TW_FLANGE_H, 0), (0, 0.1, 0), TW_FLANGE_D / 2.0))
    for i in range(12):
        a = 2 * math.pi * i / 12
        px, pz = (TW_D / 2.0) * math.cos(a), (TW_D / 2.0) * math.sin(a)
        sub(t, cyl((px, -0.5, pz), (px, TW_H + 0.5, pz), 2.20))
    sub(t, cyl((0, -TW_FLANGE_H - 0.5, 0), (0, TW_H + 0.5, 0), SCREW_CLR / 2.0))
    sub(t, hex_prism_y(0, 0, TW_HEX, TW_H - TW_HEX_D, TW_H + 0.5))
    return t


# ----------------------------------------------------------------------------
#  6.  MOCKS
# ----------------------------------------------------------------------------

def build_mock_display():
    m = box(-DISP_PCB_W / 2, PCB_Y0, Z_LEDGE, DISP_PCB_W / 2, PCB_Y1, Z_PCB_BOT)
    uni(m, box(-DISP_LCD_W / 2, LCD_CY - DISP_LCD_H / 2, Z_LCD_TOP,
               DISP_LCD_W / 2, LCD_CY + DISP_LCD_H / 2, Z_LEDGE))
    uni(m, box(-ACT_W / 2, WIN_CY - ACT_H / 2, Z_LCD_TOP + 0.1,
               ACT_W / 2, WIN_CY + ACT_H / 2, Z_LCD_TOP))
    uni(m, box(-HDR14_W / 2, PCB_Y0 + HDR14_OFF - 1.27, Z_PCB_BOT,
               HDR14_W / 2, PCB_Y0 + HDR14_OFF + 1.27, Z_PCB_BOT - HDR_BACK))
    uni(m, box(-HDR4_W / 2, PCB_Y1 - HDR4_OFF - 1.27, Z_PCB_BOT,
               HDR4_W / 2, PCB_Y1 - HDR4_OFF + 1.27, Z_PCB_BOT - HDR_BACK))
    return m


def build_mock_esp():
    m = box(-ESP_W / 2, ESP_Y0, ESP_Z0, ESP_W / 2, ESP_Y1, ESP_Z1)
    uni(m, box(-USB_W / 2 + 1.5, ESP_Y0 - 1.0, ESP_Z1,
               USB_W / 2 - 1.5, ESP_Y0 + 7.5, ESP_Z1 + 3.2))
    uni(m, cyl((EC11_X, CASE_H - WALL, EC11_Z),
               (EC11_X, CASE_H - WALL - EC11_BODY_H, EC11_Z), EC11_BODY / 2.0))
    uni(m, cyl((BUZ_X, BUZ_Y, Z_BACK_IN), (BUZ_X, BUZ_Y, Z_BACK_IN + BUZ_BODY_H),
               BUZ_D / 2.0))
    return m


# ----------------------------------------------------------------------------
#  ENTRY POINT
# ----------------------------------------------------------------------------

def run(context):
    global app, ui, des, root, tbm
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        des = adsk.fusion.Design.cast(app.activeProduct)
        if not des:
            ui.messageBox('Open a Fusion Design workspace first.')
            return
        root = des.rootComponent
        tbm = adsk.fusion.TemporaryBRepManager.get()

        # the enclosure sits PLATE_T + PIVOT_H - PIVOT_Y above the plate at 90 deg
        lift = PLATE_T + PIVOT_H - PIVOT_Y

        c = new_comp('01_FrontShell', 0, lift, 0)
        add_body(c, build_front_shell(), 'FrontShell')

        c = new_comp('02_BackShell', 0, lift, 0)
        add_body(c, build_back_shell(), 'BackShell')

        c = new_comp('03_Base')
        add_body(c, build_base(), 'Base')

        if BUILD_HARDWARE:
            c = new_comp('04_Knob', 70, 40, 0)
            add_body(c, build_knob(), 'Knob')
            c = new_comp('05_Thumbwheel', 70, 10, 0)
            add_body(c, build_thumbwheel(), 'Thumbwheel')

        if BUILD_MOCKS:
            c = new_comp('90_MockDisplay', 0, lift, 0)
            add_body(c, build_mock_display(), 'MockDisplay')
            c = new_comp('91_MockElectronics', 0, lift, 0)
            add_body(c, build_mock_esp(), 'MockElectronics')

        app.activeViewport.fit()

        ui.messageBox(
            'Pico Assistant case generated.\n\n'
            'Enclosure   {:.2f} x {:.2f} x {:.2f} mm\n'
            'Base        {:.0f} x {:.0f} x {:.0f} mm\n'
            'Window      {:.2f} x {:.2f} mm\n'
            'Tilt range  {:.0f} - {:.0f} deg, {:.0f} deg detents\n\n'
            'Hide 90_MockDisplay / 91_MockElectronics before exporting STLs.'
            .format(CASE_W, CASE_H, CASE_D,
                    2 * PLATE_X, PLATE_ZF - PLATE_ZR, PLATE_T + CHEEK_TOP,
                    ACT_W + 1.2, ACT_H + 1.2,
                    TILT_MIN, TILT_MAX, DETENT_STEP))

    except Exception:
        if ui:
            ui.messageBox('Failed:\n{}'.format(traceback.format_exc()))
