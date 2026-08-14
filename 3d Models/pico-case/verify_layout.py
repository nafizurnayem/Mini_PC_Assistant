# -*- coding: utf-8 -*-
"""
Clearance / collision checker for the Pico Assistant case.

Reads the parameter block straight out of PicoAssistantCase.py so the two can
never drift apart, rebuilds every feature as an axis-aligned bounding volume,
and reports overlaps, thin walls and the tilt sweep.

    python verify_layout.py
"""

import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'PicoAssistantCase', 'PicoAssistantCase.py')

text = open(SRC, encoding='utf-8').read()
block = text[text.index('BUILD_MOCKS'):text.index('app = None')]
P = {'math': math}
exec(block, P)
g = lambda k: P[k]

# ---------------------------------------------------------------------------
# volumes:  name -> (x0,x1, y0,y1, z0,z1, owner)
# owner "F" front shell, "B" back shell, "*" both / shared cavity
# ---------------------------------------------------------------------------

CASE_W, CASE_H, CASE_D = g('CASE_W'), g('CASE_H'), g('CASE_D')
IN_X, WALL, WALL_X = g('IN_X'), g('WALL'), g('WALL_X')
Z_SPLIT, Z_BACK_IN, Z_BACK_OUT = g('Z_SPLIT'), g('Z_BACK_IN'), g('Z_BACK_OUT')
Z_LEDGE, Z_PCB_BOT, Z_WALL_IN = g('Z_LEDGE'), g('Z_PCB_BOT'), g('Z_WALL_IN')

vols = {}


def V(name, x0, x1, y0, y1, z0, z1, owner):
    vols[name] = (min(x0, x1), max(x0, x1), min(y0, y1), max(y0, y1),
                  min(z0, z1), max(z0, z1), owner)


def cylV(name, axis, c, r, a0, a1, owner):
    """Bounding box of a cylinder; axis in 'x','y','z'; c = the two other coords."""
    if axis == 'x':
        V(name, a0, a1, c[0] - r, c[0] + r, c[1] - r, c[1] + r, owner)
    elif axis == 'y':
        V(name, c[0] - r, c[0] + r, a0, a1, c[1] - r, c[1] + r, owner)
    else:
        V(name, c[0] - r, c[0] + r, c[1] - r, c[1] + r, a0, a1, owner)


# --- display -----------------------------------------------------------------
V('display_pcb', -g('DISP_PCB_W') / 2, g('DISP_PCB_W') / 2,
  g('PCB_Y0'), g('PCB_Y1'), Z_PCB_BOT, Z_LEDGE, '*')
V('hdr14', -g('HDR14_W') / 2, g('HDR14_W') / 2,
  g('PCB_Y0') + g('HDR14_OFF') - 1.27, g('PCB_Y0') + g('HDR14_OFF') + 1.27,
  Z_PCB_BOT - g('HDR_BACK'), Z_PCB_BOT, '*')
V('hdr4', -g('HDR4_W') / 2, g('HDR4_W') / 2,
  g('PCB_Y1') - g('HDR4_OFF') - 1.27, g('PCB_Y1') - g('HDR4_OFF') + 1.27,
  Z_PCB_BOT - g('HDR_BACK'), Z_PCB_BOT, '*')

# --- front shell -------------------------------------------------------------
cylV('touch_membrane', 'z', (0, g('TOUCH_Y')), g('TOUCH_MEM_D') / 2,
     Z_WALL_IN, -(g('TOUCH_FACE_Z') + g('TOUCH_MEM_T')), 'F')
V('ttp223', -(g('TTP_W') / 2 + g('TTP_RIB') + 0.2),
  g('TTP_W') / 2 + g('TTP_RIB') + 0.2,
  g('TOUCH_Y') - (g('TTP_H') / 2 + g('TTP_RIB') + 0.2),
  g('TOUCH_Y') + (g('TTP_H') / 2 + g('TTP_RIB') + 0.2),
  -(g('TOUCH_FACE_Z') + g('TOUCH_MEM_T')) - g('TTP_RIB_H'),
  -(g('TOUCH_FACE_Z') + g('TOUCH_MEM_T')), 'F')

for i, (bx, by) in enumerate(g('BOSS_XY')):
    cylV('boss_f%d' % i, 'z', (bx, by), g('BOSS_D') / 2, Z_WALL_IN, Z_SPLIT, 'F')
    cylV('boss_b%d' % i, 'z', (bx, by), g('BOSS_D') / 2, Z_BACK_IN, Z_SPLIT, 'B')

# --- back shell --------------------------------------------------------------
for sx in (-1, 1):
    s = 'R' if sx > 0 else 'L'
    for py, nm in ((g('PIVOT_Y'), 'pivot'), (g('LOCK_Y'), 'lock')):
        cylV('insert_%s_%s' % (nm, s), 'x', (py, g('PIVOT_Z')), g('INSERT_D') / 2,
             sx * (g('PAD_X') - g('INSERT_L')), sx * g('PAD_X'), 'B')

for sx in (-1, 1):
    for py in (g('PCB_Y0') + 4.0, g('PCB_Y1') - 4.0):
        cylV('post_%d_%d' % (sx, int(py)), 'z', (sx * g('POST_X'), py),
             g('POST_D') / 2, Z_BACK_IN, Z_PCB_BOT - g('POST_GAP'), 'B')

ri = g('ESP_W') / 2 - g('RAIL_GRIP')
V('esp_rails', -(ri + g('RAIL_T')), ri + g('RAIL_T'),
  g('ESP_Y0') - 1.0, g('ESP_Y1') + 1.5, Z_BACK_IN, g('ESP_Z1') + 2.0, 'B')
V('esp_board', -g('ESP_W') / 2, g('ESP_W') / 2, g('ESP_Y0'), g('ESP_Y1'),
  g('ESP_Z0'), g('ESP_Z1') + 3.2, 'B')
V('usb_port', -g('USB_W') / 2, g('USB_W') / 2, -1.0, WALL + 1.0,
  g('ESP_Z1') - 0.2, g('ESP_Z1') + g('USB_H') - 0.2, 'B')

V('ec11_body', g('EC11_X') - g('EC11_BODY') / 2, g('EC11_X') + g('EC11_BODY') / 2,
  CASE_H - WALL - g('EC11_BODY_H') - 5.0, CASE_H - WALL,
  g('EC11_Z') - g('EC11_BODY') / 2, g('EC11_Z') + g('EC11_BODY') / 2, 'B')
cylV('ec11_bush', 'y', (g('EC11_X'), g('EC11_Z')), g('EC11_BUSH_D') / 2,
     CASE_H - WALL, CASE_H, 'B')

cylV('buzzer', 'z', (g('BUZ_X'), g('BUZ_Y')), g('BUZ_D') / 2 + g('BUZ_RING_T'),
     Z_BACK_IN, Z_BACK_IN + g('BUZ_BODY_H'), 'B')

# ---------------------------------------------------------------------------
# checks
# ---------------------------------------------------------------------------

fails, warns, notes = [], [], []


def overlap(a, b):
    ov = []
    for i in (0, 2, 4):
        lo, hi = max(a[i], b[i]), min(a[i + 1], b[i + 1])
        if hi - lo <= 0.0:
            return None
        ov.append(hi - lo)
    return ov


# pairs that are meant to touch or nest inside one another
IGNORE = {
    ('display_pcb', 'hdr14'), ('display_pcb', 'hdr4'),
    ('esp_rails', 'esp_board'), ('esp_board', 'usb_port'),
    ('esp_rails', 'usb_port'),
    ('touch_membrane', 'ttp223'),
    ('ec11_body', 'ec11_bush'),
}
# front-shell bosses and back-shell columns are coaxial and meet at the split
for _i in range(4):
    IGNORE.add(('boss_f%d' % _i, 'boss_b%d' % _i))

names = sorted(vols)
for i in range(len(names)):
    for j in range(i + 1, len(names)):
        n1, n2 = names[i], names[j]
        if (n1, n2) in IGNORE or (n2, n1) in IGNORE:
            continue
        ov = overlap(vols[n1], vols[n2])
        if ov:
            fails.append('COLLISION  %-16s <-> %-16s  overlap %.2f x %.2f x %.2f mm'
                         % (n1, n2, ov[0], ov[1], ov[2]))

# --- every internal feature must stay inside the cavity ---------------------
# (voids in the front wall and features that deliberately pierce a wall are
#  exempt from the Z test)
Z_EXEMPT = {'touch_membrane', 'ttp223', 'usb_port', 'ec11_bush'}
for n, v in sorted(vols.items()):
    if n.startswith('insert_') or n == 'ec11_bush' or n == 'usb_port':
        continue
    if v[0] < -IN_X - 1e-6 or v[1] > IN_X + 1e-6:
        fails.append('OUT OF CAVITY (X)  %-16s  %.2f .. %.2f  limit +-%.2f'
                     % (n, v[0], v[1], IN_X))
    if v[2] < WALL - 1e-6 or v[3] > CASE_H - WALL + 1e-6:
        if n != 'esp_rails' and n != 'esp_board':
            fails.append('OUT OF CAVITY (Y)  %-16s  %.2f .. %.2f' % (n, v[2], v[3]))
    if n not in Z_EXEMPT and (v[4] < Z_BACK_IN - 1e-6 or v[5] > Z_WALL_IN + 1e-6):
        fails.append('OUT OF CAVITY (Z)  %-16s  %.2f .. %.2f' % (n, v[4], v[5]))

# --- wall thicknesses -------------------------------------------------------
def chk(label, value, minimum):
    if value < minimum - 1e-9:
        fails.append('THIN  %-34s %.2f mm  (min %.2f)' % (label, value, minimum))
    else:
        notes.append('%-38s %.2f mm' % (label, value))


chk('hinge insert -> cavity wall', g('PAD_X') - g('INSERT_L') - IN_X, 0.80)
chk('front insert -> outer front face', abs(Z_SPLIT) - g('INSERT_L'), 1.50)
chk('touch membrane', g('TOUCH_MEM_T'), 1.00)
chk('front wall', -Z_WALL_IN, 2.00)
chk('back wall', Z_BACK_IN - Z_BACK_OUT, 2.00)
chk('side wall', WALL, 2.00)
chk('EC11 hole below the parting line',
    (abs(g('EC11_Z')) - g('EC11_BUSH_D') / 2) - abs(Z_SPLIT), 0.50)
chk('EC11 body -> back inner face',
    abs(Z_BACK_IN) - (abs(g('EC11_Z')) + g('EC11_BODY') / 2), 0.25)
chk('rear cavity vs header pins', g('REAR_CLEAR') - g('HDR_BACK'), 2.00)
chk('lip rebate -> PCB ledge', abs(Z_SPLIT) - g('LIP_H') - abs(Z_LEDGE), 0.50)
chk('USB port -> back inner face', g('ESP_Z1') - 0.2 - Z_BACK_IN, 1.00)
chk('buzzer cradle -> brow cavity (Y)',
    (CASE_H - WALL) - (g('BUZ_Y') + g('BUZ_D') / 2), 0.00)
chk('press post gap under the PCB', g('POST_GAP'), 0.05)

# --- tilt sweep -------------------------------------------------------------
PIVOT_H, PLATE_T = g('PIVOT_H'), g('PLATE_T')
PIVOT_Y, PIVOT_Z = g('PIVOT_Y'), g('PIVOT_Z')
corners = [(0.0, 0.0), (0.0, -CASE_D), (CASE_H, 0.0), (CASE_H, -CASE_D)]
sweep = []
for tilt in (90, 75, 60, 45):
    t = math.radians(90 - tilt)
    lowest, rear, fwd = 1e9, 1e9, -1e9
    for (cy, cz) in corners:
        dy, dz = cy - PIVOT_Y, cz - PIVOT_Z
        ry = dy * math.cos(t) + dz * math.sin(t)
        rz = -dy * math.sin(t) + dz * math.cos(t)
        lowest = min(lowest, PLATE_T + PIVOT_H + ry)
        rear = min(rear, PIVOT_Z + rz)
        fwd = max(fwd, PIVOT_Z + rz)
    sweep.append((tilt, lowest, rear, fwd))
    if lowest < PLATE_T + 1.0:
        fails.append('TILT %d deg: enclosure fouls the base plate (%.1f mm)'
                     % (tilt, lowest - PLATE_T))
    if fwd > g('PLATE_ZF'):
        fails.append('TILT %d deg: enclosure overhangs the plate front by %.1f mm'
                     % (tilt, fwd - g('PLATE_ZF')))

# centre of mass overhang at the most reclined angle
t = math.radians(90 - 45)
cy, cz = CASE_H / 2.0 - PIVOT_Y, -CASE_D / 2.0 - PIVOT_Z
com_z = PIVOT_Z + (-cy * math.sin(t) + cz * math.cos(t))
if com_z < g('PLATE_ZR') + 8.0:
    fails.append('TIP RISK: CoM at %.1f mm vs plate rear edge %.1f mm'
                 % (com_z, g('PLATE_ZR')))

# lock arc must stay clear of the plate and inside the cheek
def lock_point(tilt):
    tt = math.radians(90 - tilt)
    return (PLATE_T + PIVOT_H - g('LOCK_R') * math.cos(tt),
            PIVOT_Z - g('LOCK_R') * math.sin(tt))


for tilt in (90, 45):
    ly, lz = lock_point(tilt)
    if ly - g('SLOT_DETENT') / 2 < PLATE_T + 2.0:
        fails.append('lock slot at %d deg breaks into the plate (y=%.1f)' % (tilt, ly))
    if ly + g('SLOT_DETENT') / 2 > PLATE_T + g('CHEEK_TOP'):
        fails.append('lock slot at %d deg exits the cheek top (y=%.1f)' % (tilt, ly))

# ---------------------------------------------------------------------------
print('=' * 74)
print('PICO ASSISTANT CASE - LAYOUT VERIFICATION')
print('=' * 74)
print('Enclosure       %.2f W  x  %.2f H  x  %.2f D  mm' % (CASE_W, CASE_H, CASE_D))
print('Cavity          %.2f  x  %.2f  x  %.2f  mm'
      % (2 * IN_X, CASE_H - 2 * WALL, Z_WALL_IN - Z_BACK_IN))
print('Viewing window  %.2f x %.2f mm, centre %.2f mm above the case centre'
      % (g('ACT_W') + 1.2, g('ACT_H') + 1.2, g('WIN_CY') - CASE_H / 2))
print('Base            %.0f x %.0f x %.0f mm'
      % (2 * g('PLATE_X'), g('PLATE_ZF') - g('PLATE_ZR'), PLATE_T + g('CHEEK_TOP')))
print('Display seat    front face at z = %.2f, PCB back at z = %.2f'
      % (Z_LEDGE, Z_PCB_BOT))
print()
print('-- clearances ' + '-' * 60)
for n in notes:
    print('   ' + n)
print()
print('-- tilt sweep ' + '-' * 60)
for tilt, low, rear, fwd in sweep:
    print('   %2d deg   lowest %5.1f mm above plate bottom   reach %7.1f .. %5.1f mm'
          % (tilt, low, rear, fwd))
print('   plate spans z = %.0f .. %.0f mm' % (g('PLATE_ZF'), g('PLATE_ZR')))
print()
if warns:
    print('-- warnings ' + '-' * 62)
    for w in warns:
        print('   ' + w)
    print()
print('-- result ' + '-' * 64)
if fails:
    for f in fails:
        print('   ' + f)
    print('\n   %d PROBLEM(S)' % len(fails))
    sys.exit(1)
print('   %d volumes checked pairwise, no collisions, all walls within limits.'
      % len(vols))
print('   PASS')
