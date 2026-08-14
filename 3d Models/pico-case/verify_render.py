# -*- coding: utf-8 -*-
"""
Offline geometry preview for the Pico Assistant case.

Executes the *same* builder functions used by the Fusion script, but against a
tiny point-membership CSG kernel instead of Fusion's BRep kernel, then slices
the result and writes PNG sections.  If a boolean is wrong, it shows up here
before anything reaches Fusion or a printer.

    python verify_render.py
"""

import math
import os
import struct
import sys
import zlib

sys.setrecursionlimit(20000)

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'PicoAssistantCase', 'PicoAssistantCase.py')
OUT = os.path.join(HERE, 'preview')
os.makedirs(OUT, exist_ok=True)

src = open(SRC, encoding='utf-8').read()


def between(a, b):
    return src[src.index(a):src.index(b)]


# ---------------------------------------------------------------------------
#  minimal CSG kernel
# ---------------------------------------------------------------------------

INF = 1e9


class N(object):
    __slots__ = ('op', 'a', 'b', 'bb', 'test')

    def __init__(self, op, a=None, b=None, bb=None, test=None):
        self.op, self.a, self.b, self.bb, self.test = op, a, b, bb, test


def bb_union(p, q):
    return [min(p[0], q[0]), max(p[1], q[1]), min(p[2], q[2]),
            max(p[3], q[3]), min(p[4], q[4]), max(p[5], q[5])]


def bb_isect(p, q):
    return [max(p[0], q[0]), min(p[1], q[1]), max(p[2], q[2]),
            min(p[3], q[3]), max(p[4], q[4]), min(p[5], q[5])]


def inside(n, x, y, z):
    bb = n.bb
    if x < bb[0] or x > bb[1] or y < bb[2] or y > bb[3] or z < bb[4] or z > bb[5]:
        return False
    op = n.op
    if op == 'p':
        return n.test(x, y, z)
    if op == 'u':
        return inside(n.a, x, y, z) or inside(n.b, x, y, z)
    if op == 'd':
        return inside(n.a, x, y, z) and not inside(n.b, x, y, z)
    return inside(n.a, x, y, z) and inside(n.b, x, y, z)


def box(x0, y0, z0, x1, y1, z1):
    a = [min(x0, x1), max(x0, x1), min(y0, y1), max(y0, y1),
         min(z0, z1), max(z0, z1)]
    return N('p', bb=a, test=lambda x, y, z: True)      # bbox test is enough


def _axis(p0, p1):
    d = [p1[i] - p0[i] for i in range(3)]
    for i in range(3):
        if abs(d[i]) > 1e-9 and abs(d[(i + 1) % 3]) < 1e-9 \
                and abs(d[(i + 2) % 3]) < 1e-9:
            return i
    raise ValueError('non axis-aligned cylinder: %s -> %s' % (p0, p1))


def cone(p0, r0, p1, r1):
    ax = _axis(p0, p1)
    u, v = [i for i in range(3) if i != ax]
    a0, a1 = p0[ax], p1[ax]
    lo, hi = min(a0, a1), max(a0, a1)
    cu, cv = p0[u], p0[v]
    rmax = max(r0, r1)
    bb = [0] * 6
    bb[2 * ax], bb[2 * ax + 1] = lo, hi
    bb[2 * u], bb[2 * u + 1] = cu - rmax, cu + rmax
    bb[2 * v], bb[2 * v + 1] = cv - rmax, cv + rmax

    def test(x, y, z, ax=ax, u=u, v=v, a0=a0, a1=a1, r0=r0, r1=r1,
             cu=cu, cv=cv):
        p = (x, y, z)
        t = 0.0 if abs(a1 - a0) < 1e-12 else (p[ax] - a0) / (a1 - a0)
        r = r0 + (r1 - r0) * t
        return (p[u] - cu) ** 2 + (p[v] - cv) ** 2 <= r * r

    return N('p', bb=bb, test=test)


def cyl(p0, p1, r):
    return cone(p0, r, p1, r)


def uni(a, b):
    n = N('u', N(a.op, a.a, a.b, a.bb, a.test), b, bb_union(a.bb, b.bb))
    a.op, a.a, a.b, a.bb, a.test = n.op, n.a, n.b, n.bb, n.test
    return a


def sub(a, b):
    n = N('d', N(a.op, a.a, a.b, a.bb, a.test), b, list(a.bb))
    a.op, a.a, a.b, a.bb, a.test = n.op, n.a, n.b, n.bb, n.test
    return a


def isect(a, b):
    n = N('i', N(a.op, a.a, a.b, a.bb, a.test), b, bb_isect(a.bb, b.bb))
    a.op, a.a, a.b, a.bb, a.test = n.op, n.a, n.b, n.bb, n.test
    return a


# ---------------------------------------------------------------------------
#  load parameters, shape helpers and builders straight from the Fusion script
# ---------------------------------------------------------------------------

G = {'math': math, 'box': box, 'cyl': cyl, 'cone': cone,
     'uni': uni, 'sub': sub, 'isect': isect}

exec(between('BUILD_MOCKS', 'app = None'), G)
exec(between('def rr_z', 'def hex_prism_y'), G)


def hex_prism_y(cx, cz, af, y0, y1):
    a = af / 2.0
    bb = [cx - af, cx + af, min(y0, y1), max(y0, y1), cz - af, cz + af]

    def test(x, y, z):
        dx, dz = x - cx, z - cz
        for k in range(3):
            t = math.radians(60.0 * k)
            if abs(-dx * math.sin(t) + dz * math.cos(t)) > a:
                return False
        return True

    return N('p', bb=bb, test=test)


G['hex_prism_y'] = hex_prism_y
exec(between('#  1.  FRONT SHELL', '#  ENTRY POINT'), G)

front = G['build_front_shell']()
back = G['build_back_shell']()
base = G['build_base']()
knob = G['build_knob']()
wheel = G['build_thumbwheel']()

g = G.get

# ---------------------------------------------------------------------------
#  PNG output
# ---------------------------------------------------------------------------

def write_png(path, w, h, pix):
    raw = b''.join(b'\x00' + bytes(pix[r * w * 3:(r + 1) * w * 3])
                   for r in range(h))

    def chunk(t, d):
        c = t + d
        return struct.pack('>I', len(d)) + c + \
            struct.pack('>I', zlib.crc32(c) & 0xffffffff)

    hdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    with open(path, 'wb') as fh:
        fh.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', hdr) +
                 chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


BG = (24, 26, 30)
GRID = (44, 48, 56)
SOLID = [(126, 214, 255), (255, 176, 96), (150, 245, 170), (240, 130, 200)]
GAP = 10


def render(panels, path, scale=4, pad=6):
    """panels: (title, solids[], u_rng, v_rng, point_fn)

    A "solid" is either a CSG node (sampled with the panel's point_fn) or a
    (node, point_fn) pair carrying its own placement - used for the assembled
    elevations, where the enclosure is rotated but the base is not."""
    ws, hs = [], []
    for _, _, ur, vr, _ in panels:
        ws.append(int((ur[1] - ur[0]) * scale) + 2 * pad)
        hs.append(int((vr[1] - vr[0]) * scale) + 2 * pad)
    W = sum(ws) + GAP * (len(panels) - 1)
    H = max(hs)
    pix = bytearray(BG * (W * H))

    xoff = 0
    for (title, solids, ur, vr, pf), pw, ph in zip(panels, ws, hs):
        nu = int((ur[1] - ur[0]) * scale)
        nv = int((vr[1] - vr[0]) * scale)
        for j in range(nv):
            v = vr[1] - (j + 0.5) / scale
            base_i = (j + pad) * W + xoff + pad
            for i in range(nu):
                u = ur[0] + (i + 0.5) / scale
                col = None
                for k, s in enumerate(solids):
                    node, spf = s if isinstance(s, tuple) else (s, pf)
                    x, y, z = spf(u, v)
                    if inside(node, x, y, z):
                        col = SOLID[k % len(SOLID)]
                        break
                if col is None:
                    if abs(u % 10) < 1.0 / scale or abs(v % 10) < 1.0 / scale:
                        col = GRID
                    else:
                        continue
                o = (base_i + i) * 3
                pix[o], pix[o + 1], pix[o + 2] = col
        xoff += pw + GAP
    write_png(path, W, H, pix)
    print('  %-42s %4d x %4d px' % (os.path.basename(path), W, H))


CW, CH, CD = g('CASE_W'), g('CASE_H'), g('CASE_D')
HW = CW / 2 + 2

print('rendering sections ...')

# --- front shell, XY slices -------------------------------------------------
render([('z=%.1f' % z, [front], (-HW, HW), (-2, CH + 2),
         (lambda u, v, z=z: (u, v, z)))
        for z in (-0.5, -3.5, -6.0, -7.5)],
       os.path.join(OUT, '01_front_shell_slices.png'))

# --- back shell, XY slices --------------------------------------------------
render([('z=%.1f' % z, [back], (-HW - 5, HW + 5), (-2, CH + 2),
         (lambda u, v, z=z: (u, v, z)))
        for z in (-7.0, -11.0, -16.5, -19.0)],
       os.path.join(OUT, '02_back_shell_slices.png'))

# --- enclosure vertical sections (Y up, Z across) ---------------------------
render([('x=%.0f' % x, [front, back], (-CD - 2, 2), (-2, CH + 2),
         (lambda u, v, x=x: (x, v, u)))
        for x in (0.0, 11.0, 24.0, 29.0)],
       os.path.join(OUT, '03_enclosure_sections.png'), scale=6)

# --- base -------------------------------------------------------------------
PX, ZF, ZR = g('PLATE_X'), g('PLATE_ZF'), g('PLATE_ZR')
render([('plan y=3', [base], (-PX - 2, PX + 2), (ZR - 2, ZF + 2),
         (lambda u, v: (u, 3.0, v))),
        ('plan y=25', [base], (-PX - 2, PX + 2), (ZR - 2, ZF + 2),
         (lambda u, v: (u, 25.0, v))),
        ('cheek x=37', [base], (ZR - 2, ZF + 2), (-2, 62),
         (lambda u, v: (37.0, v, u))),
        ('front x-y', [base], (-PX - 2, PX + 2), (-2, 62),
         (lambda u, v: (u, v, -11.0)))],
       os.path.join(OUT, '04_base.png'))

# --- hardware ---------------------------------------------------------------
render([('knob y=4', [knob], (-16, 16), (-16, 16),
         (lambda u, v: (u, 4.0, v))),
        ('knob y=12', [knob], (-16, 16), (-16, 16),
         (lambda u, v: (u, 12.0, v))),
        ('knob sect', [knob], (-16, 16), (-3, 17),
         (lambda u, v: (u, v, 0.0))),
        ('wheel y=8', [wheel], (-13, 13), (-13, 13),
         (lambda u, v: (u, 8.0, v))),
        ('wheel sect', [wheel], (-13, 13), (-3, 11),
         (lambda u, v: (u, v, 0.0)))],
       os.path.join(OUT, '05_hardware.png'), scale=6)

# --- assembled side elevation at each detent --------------------------------
PLATE_T, PIVOT_H = g('PLATE_T'), g('PIVOT_H')
PIVOT_Y, PIVOT_Z = g('PIVOT_Y'), g('PIVOT_Z')
LIFT = PLATE_T + PIVOT_H - PIVOT_Y


def tilted(tilt):
    t = math.radians(90 - tilt)
    py, pz = PLATE_T + PIVOT_H, PIVOT_Z
    ct, st = math.cos(t), math.sin(t)

    def pf(u, v):
        dy, dz = v - py, u - pz
        ry = dy * ct - dz * st
        rz = dy * st + dz * ct
        return (0.0, ry + PIVOT_Y, rz + PIVOT_Z)
    return pf


def base_at_x(x):
    return lambda u, v: (x, v, u)


panels = []
for t in (90, 75, 60, 45):
    panels.append(('%d deg' % t,
                   [(front, tilted(t)), (back, tilted(t)),
                    (base, base_at_x(37.0))],
                   (ZR - 10, ZF + 10), (-2, 180), tilted(t)))
render(panels, os.path.join(OUT, '06_tilt_positions.png'), scale=3)

print('done -> %s' % OUT)
