"""Goli desk robot -- parametric model for Bambu Lab A1 mini (180 mm cube).

Face: Waveshare ESP32-S3-Touch-AMOLED-1.75 (round 466x466 AMOLED, ESP32-S3,
IMU, mic, speaker driver, LiPo charger). Glass is Ø48.96 x 2.05 mm, PCB Ø46,
M2 nuts (H3.5) soldered on the back at (0,20.5) and (±13.75,-14.7); the nut
tops sit 10.4 mm behind the glass front.

Run:  python goli_bot.py && python make_viewer.py   -> STLs in ./stl, viewer.html
Units: mm.  x = right, y = toward the viewer (front), z = up.
"""
import math
import os

import numpy as np
import trimesh
import functools

from manifold3d import CrossSection, JoinType, Manifold, Mesh, OpType

OUT = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------- parameters
S = 0.32                # mm per unit of the Goli SVG artwork
SY_BED = 548            # SVG y of the print bed (egg bottom is 568)
K = 0.80                # body depth / width
N_SE = 2.6              # superellipse exponent of the horizontal section
WALL = 2.6
FLOOR = 2.4
BACK_K = 0.50           # flat back at y = -BACK_K * max depth

FACE_SY = 315           # SVG y of the face centre
FACE_TILT = 10          # face looks up a little (deg)

# display (Waveshare ESP32-S3-Touch-AMOLED-1.75)
GLASS_R = 48.96 / 2
VA_R = 44.16 / 2
NUT_DEPTH = 10.4        # glass front -> back nut tops
DISP_ROT = -90          # board rotated so USB-C points down
PCB_NUTS = [(0, 20.5), (13.75, -14.7), (-13.75, -14.7)]
USB_HALF_W = 7.0        # plug clearance

# optional battery: a 1000 mAh LiPo, size 103450 (50 x 34 x 10 mm), on a cradle above the floor's curve
BATTERY = (50.0, 34.0, 10.0)
BATT_Z = 6.0            # its underside (the floor curves up at the front corners below this)

# bezel
LIP_R = VA_R + 0.25     # visible opening
LIP_T = 1.6
BORE_R = GLASS_R + 0.3  # glass / PCB pocket
TUBE_R = 29.6
FLANGE_R = 30.9
FLANGE_T = 3.0
BEZEL_SCREW_R = 27.1
BEZEL_SCREW_ANG = [90, 210, 330]
TUBE_BACK = -(NUT_DEPTH + 0.05)      # w of the bracket front face
SOCKET_R = 34.5
FIT = 0.25

SEG = 96
Y_AXIS = 39.0           # set by build()


# ---------------------------------------------------------------- helpers
def bez(p0, p1, p2, p3, n):
    t = np.linspace(0, 1, n)[:, None]
    return ((1 - t) ** 3) * p0 + 3 * ((1 - t) ** 2) * t * p1 + 3 * (1 - t) * t * t * p2 + t ** 3 * p3


def svg_x(sx):
    # the viewer stands at +y, so the artwork's left is +x
    return (320 - sx) * S


def svg_z(sy):
    return (SY_BED - sy) * S


def profile_points():
    """Right half of Goli's silhouette (SVG path of the body), as (r, z)."""
    a = bez(np.array([320, 172.]), np.array([445, 172.]), np.array([488, 285.]), np.array([488, 405.]), 120)
    b = bez(np.array([488, 405.]), np.array([488, 525.]), np.array([425, 568.]), np.array([320, 568.]), 120)
    p = np.vstack([a, b[1:]])
    return np.c_[(p[:, 0] - 320) * S, svg_z(p[:, 1])]


@functools.lru_cache(maxsize=None)
def radius_fn(offset):
    """r(z) of the silhouette grown by `offset` (2D offset, round joins)."""
    p = profile_points()
    full = np.vstack([p, np.c_[-p[::-1, 0], p[::-1, 1]]])[::-1]  # counter-clockwise
    cs = CrossSection([full.tolist()]).offset(offset, JoinType.Round, 2.0, 64) if offset else CrossSection([full.tolist()])
    pts = np.vstack([np.array(poly) for poly in cs.to_polygons()])
    zmin, zmax = pts[:, 1].min(), pts[:, 1].max()
    # r(z) = right-most crossing of the (convex) outline at each height
    zs = np.linspace(zmin, zmax, 400)
    rr = []
    poly = np.array(cs.to_polygons()[0])
    for z in zs:
        best = 0.0
        for i in range(len(poly)):
            (x1, z1), (x2, z2) = poly[i], poly[(i + 1) % len(poly)]
            if (z1 - z) * (z2 - z) <= 0 and z1 != z2:
                x = x1 + (z - z1) * (x2 - x1) / (z2 - z1)
                best = max(best, x)
        rr.append(best)
    return zs, np.array(rr), zmin, zmax


_cache = {}


def egg(offset=0.0, segs=SEG):
    """Superellipse egg, optionally grown/shrunk by `offset`."""
    key = (round(offset, 3), segs)
    if key in _cache:
        return _cache[key]
    zs, rs, zmin, zmax = radius_fn(round(offset, 3))
    th = np.linspace(0, 2 * np.pi, segs, endpoint=False)
    c, s = np.cos(th), np.sin(th)
    ex = np.sign(c) * np.abs(c) ** (2 / N_SE)
    ey = np.sign(s) * np.abs(s) ** (2 / N_SE)
    # cosine spacing -> more rings near the poles
    u = (1 - np.cos(np.linspace(0, np.pi, 90))) / 2
    zr = zmin + u * (zmax - zmin)
    verts, faces = [[0, 0, zmin]], []
    rings = []
    for z in zr[1:-1]:
        a = float(np.interp(z, zs, rs))
        b = K * a
        start = len(verts)
        verts += [[a * ex[i], b * ey[i], z] for i in range(segs)]
        rings.append(start)
    top = len(verts)
    verts.append([0, 0, zmax])
    for i in range(segs):
        j = (i + 1) % segs
        faces.append([0, rings[0] + j, rings[0] + i])
    for r0, r1 in zip(rings[:-1], rings[1:]):
        for i in range(segs):
            j = (i + 1) % segs
            faces.append([r0 + i, r0 + j, r1 + j])
            faces.append([r0 + i, r1 + j, r1 + i])
    for i in range(segs):
        j = (i + 1) % segs
        faces.append([rings[-1] + i, rings[-1] + j, top])
    m = Manifold(Mesh(vert_properties=np.array(verts, np.float32), tri_verts=np.array(faces, np.uint32)))
    assert m.status().name == "NoError", m.status()
    _cache[key] = m
    return m


def surface_r(z, offset=0.0):
    zs, rs, _, _ = radius_fn(round(offset, 3))
    return float(np.interp(z, zs, rs))


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def cyl_y(r, y0, y1, x=0.0, z=0.0, r2=None, seg=SEG):
    """Cylinder along +y."""
    c = Manifold.cylinder(y1 - y0, r, r if r2 is None else r2, seg)
    return c.rotate([-90, 0, 0]).translate([x, y0, z])


def _extrude_y(cs, y0, y1):
    # extrude along +z, then map (x, v, w) -> (x, w, v): rotate +90 about x gives (x, -w, v)
    m = cs.extrude(y1 - y0).rotate([90, 0, 0])      # y in [-(y1-y0), 0], z = v
    return m.translate([0, y1, 0])


def ellipse_cs(cx, cz, rx, rz, seg=96):
    return CrossSection.circle(1.0, seg).scale([rx, rz]).translate([cx, cz])


def stroke(points, w):
    parts = []
    for p, q in zip(points[:-1], points[1:]):
        parts.append(CrossSection.batch_hull([CrossSection.circle(w / 2, 24).translate(p),
                                              CrossSection.circle(w / 2, 24).translate(q)]))
    return CrossSection.batch_boolean(parts, OpType.Add)


def emboss(cs, h, base=0.0):
    """Relief of height h on the body front (base = start height above skin)."""
    slab = _extrude_y(cs, 0, 120)
    return slab ^ egg(h / K) - egg(base / K)


# ---------------------------------------------------------------- face frame
FACE_Z = svg_z(FACE_SY)


def face_frame(y_axis):
    """Transform local (u right, w forward, v up) -> world for the tilted face."""
    def place(m):
        return m.rotate([FACE_TILT, 0, 0]).translate([0, y_axis, FACE_Z])
    return place


def local_cyl(r, w0, w1, r2=None, seg=SEG):
    return cyl_y(r, w0, w1, r2=r2, seg=seg)


def implicit_inside(p):
    x, y, z = p
    a = surface_r(z)
    if a <= 0:
        return False
    return abs(x / a) ** N_SE + abs(y / (K * a)) ** N_SE <= 1


def solve_face_axis():
    """Place the face so the socket front plane is just proud of the body at its rim."""
    tilt = math.radians(FACE_TILT)
    wv = np.array([0, math.cos(tilt), math.sin(tilt)])   # forward
    vv = np.array([0, -math.sin(tilt), math.cos(tilt)])  # up
    uv = np.array([1, 0, 0])
    w_front = LIP_T - FLANGE_T  # socket front plane (= flange back), local w
    for y_axis in np.linspace(0, 50, 501):
        o = np.array([0, y_axis, FACE_Z])
        worst = 1e9
        for ang in np.linspace(0, 2 * np.pi, 48, endpoint=False):
            p = o + w_front * wv + SOCKET_R * (math.cos(ang) * uv + math.sin(ang) * vv)
            # distance from p forward along w to leave the body (negative if p is outside)
            t = 0.0
            if implicit_inside(p):
                while implicit_inside(p + t * wv) and t < 30:
                    t += 0.05
                worst = min(worst, -t)
            else:
                while not implicit_inside(p - t * wv) and t < 30:
                    t += 0.05
                worst = min(worst, t)
        # want the rim to stand at most ~0.6 mm proud at its lowest point
        if worst >= 0.6:
            return y_axis
    raise RuntimeError("face does not fit")


# ---------------------------------------------------------------- build
def build():
    outer = egg(0)
    zs, rs, zmin, zmax = radius_fn(0)
    z_top = zmax
    r_max = rs.max()
    y_back = -BACK_K * K * r_max
    big = 400

    ground = box(-big, big, -big, big, 0, big)
    behind = box(-big, big, y_back, big, -big, big)

    # ---- cavity: inner egg, 45° roof, floor, open to the back
    inner = egg(-WALL)
    roof_apex = z_top - WALL - 9
    roof = Manifold.cylinder(roof_apex, roof_apex, 0, SEG)  # cone, apex at roof_apex
    cavity = inner ^ roof ^ box(-big, big, y_back - 5, big, FLOOR, big)

    # ---- face socket (tilted), placed so its front plane meets the body rim
    y_axis = solve_face_axis()
    place = face_frame(y_axis)
    w_socket_front = LIP_T - FLANGE_T
    sock_profile = CrossSection.batch_hull([
        CrossSection.circle(SOCKET_R, SEG),
        CrossSection.square([1, 1], True).translate([0, -SOCKET_R * 1.42])])  # 45° keel for printing
    socket_local = _extrude_y(sock_profile, TUBE_BACK, w_socket_front)
    # 45° collar so the face blends into the body instead of sitting on it like a coin
    collar = local_cyl(SOCKET_R + 10, w_socket_front - 10, w_socket_front, r2=SOCKET_R)
    socket_inside = place(socket_local + collar) ^ inner
    socket = (place(local_cyl(SOCKET_R, TUBE_BACK, w_socket_front) + collar) - inner) + socket_inside
    face_clear = place(local_cyl(SOCKET_R + 0.01, w_socket_front, 60))
    # bore: round, but capped teardrop top so it prints without supports
    bore_cs = CrossSection.batch_hull([
        CrossSection.circle(TUBE_R + FIT, SEG),
        CrossSection.square([2 * (TUBE_R + FIT) * 0.42, 1], True).translate([0, TUBE_R + FIT + 4])])
    bore_cs = bore_cs ^ CrossSection.square([200, 2 * (TUBE_R + FIT) + 1.0], True).translate([0, 0])
    bore_cs = (bore_cs ^ CrossSection.circle(FLANGE_R - 0.4, SEG)) + CrossSection.circle(TUBE_R + FIT, SEG)  # hidden by the flange
    bore = place(_extrude_y(bore_cs, TUBE_BACK - 20, 60))

    # ---- details on the body
    ears = []
    for sgn in (-1, 1):
        base = Manifold.sphere(6.8, 48).scale([1, 0.8, 1]).translate([sgn * 21.5, 0, z_top - 11])
        tip = Manifold.sphere(2.3, 32).translate([sgn * 30.5, 0, z_top + 4])
        ears.append(Manifold.batch_hull([base, tip]))
    arms = []
    z_arm = svg_z(430)
    for sgn in (-1, 1):
        x = sgn * (surface_r(z_arm) - 3.0)
        arm = Manifold.sphere(1.0, 64).scale([8.5, 9.5, 19]).rotate([0, sgn * -22, 0]).translate([x, 6, z_arm])
        arms.append(arm)
    feet, toes = [], []
    for sgn in (-1, 1):
        cx, rx, ry, rz = sgn * 62 * S, 56 * S, 48 * S, 25 * S
        yc = 0.21 * K * r_max
        foot = Manifold.sphere(1.0, 64).scale([rx, ry, rz]).translate([cx, yc, 0])
        feet.append(foot)
        skin = foot - Manifold.sphere(1.0, 64).scale([rx - 0.8, ry - 0.8, rz - 0.8]).translate([cx, yc, 0])
        for dx in (-5.0, 0, 5.0):
            toes.append(box(cx + dx - 0.55, cx + dx + 0.55, yc + ry * 0.4, yc + 40, 1.4, 5.2) ^ skin)

    # antenna stalk + bell peg (prints with the body)
    stalk_len, peg_len = 16.0, 6.0
    stalk = Manifold.cylinder(stalk_len + 5, 2.6, 2.4, 48).translate([0, 0, -5])
    stalk += Manifold.sphere(3.4, 48).translate([0, 0, stalk_len])  # clapper knob
    stalk += Manifold.cylinder(peg_len + 3.4, 2.0, 2.0, 48).translate([0, 0, stalk_len])
    stalk = stalk.rotate([0, 14, 0]).translate([0, 0, z_top - 1.5])

    # ---- belly (cream), badge + cheeks (red)
    belly_cz, belly_rx, belly_rz = svg_z(482), 100 * S, 62 * S
    belly_cs = ellipse_cs(0, belly_cz, belly_rx, belly_rz)
    belly = emboss(belly_cs, 1.2)
    dashes = []
    n = 44
    for i in range(n):
        t = 2 * math.pi * (i + 0.25) / n
        p = (0.89 * belly_rx * math.cos(t), belly_cz + 0.87 * belly_rz * math.sin(t))
        d = (-0.89 * belly_rx * math.sin(t), 0.87 * belly_rz * math.cos(t))
        L = math.hypot(*d)
        d = (d[0] / L * 0.9, d[1] / L * 0.9)
        dashes.append(stroke([(p[0] - d[0], p[1] - d[1]), (p[0] + d[0], p[1] + d[1])], 0.9))
    dash_cs = CrossSection.batch_boolean(dashes, OpType.Add)
    belly = belly - (_extrude_y(dash_cs, 0, 120) - egg(0.7 / K))

    card_x0, card_x1 = sorted((svg_x(262), svg_x(378)))
    card_z0, card_z1 = svg_z(498), svg_z(448)
    card_cs = CrossSection.square([card_x1 - card_x0, card_z1 - card_z0]).translate([card_x0, card_z0])
    card_cs = card_cs.offset(-3, JoinType.Round).offset(3, JoinType.Round)
    card = emboss(card_cs, 2.2)
    # speaker grille = the two "message lines" of the card
    line1 = stroke([(svg_x(282), svg_z(465)), (svg_x(336), svg_z(465))], 2.0)
    line2 = stroke([(svg_x(282), svg_z(482)), (svg_x(356), svg_z(482))], 2.0)
    grille = _extrude_y(CrossSection.batch_boolean([line1, line2], OpType.Add), 0, 120)
    badge_c = (svg_x(374), svg_z(448))
    badge = emboss(CrossSection.circle(15 * S, 64).translate(badge_c), 2.8)
    bang = CrossSection.batch_boolean([
        stroke([(badge_c[0], badge_c[1] + 1.6), (badge_c[0], badge_c[1] + 0.0)], 1.2),
        CrossSection.circle(0.6, 16).translate([badge_c[0], badge_c[1] - 1.5])],
        OpType.Add)
    bang_cut = _extrude_y(bang, 0, 120) - egg(2.2 / K)
    badge = badge - bang_cut
    cheeks = []
    for sgn in (-1, 1):
        cheeks.append(emboss(ellipse_cs(sgn * 38.0, svg_z(362), 5.6, 3.0), 0.8))
    cheeks = Manifold.batch_boolean(cheeks, OpType.Add)

    # ---- back-plate fixings: two floor pillars + a top boss under the roof
    sec = (cavity ^ box(-big, big, y_back, y_back + 0.2, -big, big))
    lo = (cavity ^ box(-big, big, y_back, y_back + 0.2, 6.0, 6.2)).bounding_box()
    x_pillar = lo[3] - 3.6
    pillars = []
    screws = []
    for sgn in (-1, 1):
        pillars.append(cavity ^ box(sgn * x_pillar - 4.2 if sgn > 0 else -big,
                                    big if sgn > 0 else sgn * x_pillar + 4.2,
                                    y_back - 1, y_back + 10, 0, 10.6))
        screws.append((sgn * x_pillar, 6.4))
    roof_low = Manifold.cylinder(roof_apex - 13, roof_apex - 13, 0, SEG)
    top_boss = (cavity - roof_low) ^ box(-6, 6, y_back - 1, y_back + 9, 0, big)
    z_top_screw = roof_apex + y_back - 6.0     # roof is 45°: at y_back it is |y_back| lower
    screws.append((0.0, z_top_screw))
    fixings = Manifold.batch_boolean(pillars + [top_boss], OpType.Add)
    screw_holes = Manifold.batch_boolean(
        [cyl_y(0.85, y_back - 1, y_back + 9, x=x, z=z, seg=24) for x, z in screws],
        OpType.Add)

    # ---- battery cradle: three rails lift it clear of the floor's curve, low stops on each side keep
    # it in place (a strip of double-sided tape holds it down); prints with the body, no supports
    bw, bd, bh = BATTERY
    by0 = y_back + 10.3                         # just in front of the back plate's screw pillars
    top = BATT_Z + 4.0                          # the stops reach 4 mm up its sides
    cradle = [box(x - 1.5, x + 1.5, by0, by0 + bd, FLOOR - 0.1, BATT_Z) for x in (-0.36 * bw, 0, 0.36 * bw)]
    for sx in (-1, 1):
        x_in = sx * (bw / 2 + 0.6)
        cradle.append(box(min(x_in, x_in + sx * 1.6), max(x_in, x_in + sx * 1.6),
                          by0 + bd / 2 - 6, by0 + bd / 2 + 6, FLOOR - 0.1, top))
    cradle.append(box(-6, 6, by0 - 2.2, by0 - 0.6, FLOOR - 0.1, top))           # behind it
    cradle.append(box(-6, 6, by0 + bd + 0.6, by0 + bd + 2.2, FLOOR - 0.1, top))  # in front
    cradle = Manifold.batch_boolean(cradle, OpType.Add) ^ cavity                  # never through the wall
    fixings = fixings + cradle                  # kept when the cavity is hollowed out

    # ---- body assembly
    body = outer + Manifold.batch_boolean(ears + arms + feet, OpType.Add) + stalk
    body = body - face_clear + socket + socket_inside
    body = body - (cavity - fixings - socket_inside - place(local_cyl(SOCKET_R, TUBE_BACK, 0)))
    body = body - bore - screw_holes - grille
    body = body - Manifold.batch_boolean(toes, OpType.Add)
    body = body ^ ground ^ behind
    # USB / power cable notch is in the back plate; nothing else here

    cream = (belly + card) - body - badge
    cream = cream - grille
    red = (badge + cheeks) - body
    cream = cream ^ ground
    red = red ^ ground

    # ---- back plate (flat, prints face down)
    plate = outer ^ box(-big, big, y_back - 2.4, y_back, 0, big)
    open_sec = (body ^ box(-big, big, y_back, y_back + 0.3, -big, big))
    hole_region = (outer ^ box(-big, big, y_back, y_back + 0.3, 0, big)) - open_sec
    # lip: follow the opening, 0.25 mm clearance, 1.8 mm wide, 3 mm deep
    hole_cs = hole_region.rotate([90, 0, 0]).slice(y_back + 0.15)   # +90 about x: z' = y
    hole_cs = max(hole_cs.decompose(), key=lambda c: c.area())      # drop slivers / screw holes
    lip_cs = hole_cs.offset(-FIT, JoinType.Round) - hole_cs.offset(-FIT - 1.8, JoinType.Round)
    lip = lip_cs.extrude(3.0).translate([0, 0, y_back]).rotate([-90, 0, 0])
    # keep the lip clear of the screw bosses
    plate = plate + (lip - fixings.translate([0, 0, 0]) - Manifold.batch_boolean(
        [cyl_y(5.2, y_back - 1, y_back + 9, x=x, z=z) for x, z in screws], OpType.Add))
    for x, z in screws:
        plate = plate - cyl_y(1.15, y_back - 5, y_back + 5, x=x, z=z, seg=24)
        plate = plate - cyl_y(2.2, y_back - 5, y_back - 1.2, x=x, z=z, seg=32)  # head recess
    # cable slot (USB-C plug passes through, 13 x 7 rounded) + vent slots
    usb = CrossSection.square([13, 7], True).offset(-1.5, JoinType.Round).offset(1.5, JoinType.Round).translate([0, 18])
    plate = plate - _extrude_y(usb, y_back - 10, y_back + 10)
    for i in range(-2, 3):
        slot = stroke([(i * 6.0, 52), (i * 6.0, 66)], 2.4)
        plate = plate - _extrude_y(slot, y_back - 10, y_back + 10)

    # ---- face bezel (navy) in local face coords, w forward
    lip_ring = local_cyl(FLANGE_R, -0.0, LIP_T) - local_cyl(LIP_R, -1, LIP_T + 1)
    # soft chamfer on the front outer edge
    lip_ring = lip_ring ^ local_cyl(FLANGE_R + 1.2, -10, LIP_T + 0.01, r2=FLANGE_R - 0.6)
    flange = local_cyl(FLANGE_R, w_socket_front, 0.01) - local_cyl(BORE_R, -1, 1)
    tube = local_cyl(TUBE_R, TUBE_BACK, w_socket_front + 0.01) - local_cyl(BORE_R, TUBE_BACK - 1, 1)
    bezel = lip_ring + flange + tube
    for a in BEZEL_SCREW_ANG:
        x, z = BEZEL_SCREW_R * math.cos(math.radians(a)), BEZEL_SCREW_R * math.sin(math.radians(a))
        bezel = bezel - cyl_y(0.85, TUBE_BACK - 1, TUBE_BACK + 8, x=x, z=z, seg=24)
    # USB-C notch at the bottom (board rotated so the port points down)
    bezel = bezel - box(-USB_HALF_W, USB_HALF_W, TUBE_BACK - 1, -2.6, -TUBE_R - 2, -BORE_R + 0.5)

    # ---- display bracket: screws to the board's nuts, clamps the bezel to the body.
    # Ring stays outside the PCB (r > 24); spokes are recessed 1.5 mm so they clear
    # the SPK/BAT connectors; only the nut pads touch the board.
    TB = TUBE_BACK
    bracket = local_cyl(SOCKET_R - 0.5, TB - 2.4, TB) - local_cyl(BORE_R - 0.3, TB - 5, TB + 1)
    th = math.radians(DISP_ROT)
    nuts = [(x * math.cos(th) - z * math.sin(th), x * math.sin(th) + z * math.cos(th)) for x, z in PCB_NUTS]
    for x, z in nuts:
        ang = math.degrees(math.atan2(z, x))
        spoke = CrossSection.square([7, 4.4], True).translate([3.5, 0]).rotate(ang).translate([x, z])
        bracket = bracket + _extrude_y(spoke, TB - 3.9, TB - 1.5) + cyl_y(2.8, TB - 3.9, TB, x=x, z=z, seg=40)
    for x, z in nuts:
        bracket = bracket - cyl_y(1.15, TB - 5, TB + 1, x=x, z=z, seg=24)
    for a in BEZEL_SCREW_ANG:
        x, z = BEZEL_SCREW_R * math.cos(math.radians(a)), BEZEL_SCREW_R * math.sin(math.radians(a))
        bracket = bracket - cyl_y(1.15, TB - 5, TB + 1, x=x, z=z, seg=24)
    bracket = bracket - box(-USB_HALF_W - 1, USB_HALF_W + 1, TB - 5, TB + 1, -big, -BORE_R + 4)

    # ---- bell (prints upright on its flat lip)
    bp = np.vstack([
        bez(np.array([26, 120.]), np.array([22, 118.]), np.array([20, 116.]), np.array([20, 112.]), 8),
        bez(np.array([20, 112.]), np.array([20, 92.]), np.array([20, 78.]), np.array([0, 76.]), 30)[1:]])
    bell_prof = [[0, 0]] + [[r * S, (120 - y) * S] for r, y in bp] + [[0, (120 - 76) * S]]
    bell_prof = [[max(x, 0.0), z] for x, z in bell_prof]
    bell = Manifold.revolve(CrossSection([bell_prof]), 96)
    bell = bell + Manifold.cylinder(1.6, 26 * S, 26 * S, 96)
    bell = bell - Manifold.cylinder(peg_len + 0.4, 2.2, 2.2, 32).translate([0, 0, -0.01])
    bell = bell - Manifold.cylinder(0.6, 2.8, 2.2, 32).translate([0, 0, -0.01])  # entry chamfer

    # ---- coffee mug (#4) for the left hand
    mug = Manifold.cylinder(15, 6.2, 6.6, 64) - Manifold.cylinder(15, 5.0, 5.4, 64).translate([0, 0, 1.4])
    handle = (Manifold.cylinder(2.2, 4.4, 4.4, 48) - Manifold.cylinder(3, 2.6, 2.6, 48).translate([0, 0, -0.4]))
    handle = handle.rotate([90, 0, 0]).translate([6.6, 1.1, 8]) ^ box(6.0, 20, -5, 5, 0, 20)
    mug = mug + handle

    assembled = {
        "body_blue": body,
        "belly_cream": cream,
        "accents_red": red,
        "backplate_blue": plate,
        "bezel_navy": place(bezel),
        "bracket_any": place(bracket),
        "bell_yellow": bell.rotate([0, 14, 0]).translate(
            [(stalk_len + 0.3) * math.sin(math.radians(14)), 0, z_top - 1.5 + (stalk_len + 0.3) * math.cos(math.radians(14))]),
        "mug_white": mug.rotate([0, 0, 200]).translate([surface_r(z_arm) + 9, 12, 4]),
    }
    print_poses = {
        "body_blue": body, "belly_cream": cream, "accents_red": red,
        "backplate_blue": plate.rotate([90, 0, 0]),
        "bezel_navy": bezel.rotate([-90, 0, 0]),
        "bracket_any": bracket.rotate([-90, 0, 0]),
        "bell_yellow": bell, "mug_white": mug,
    }
    global Y_AXIS
    Y_AXIS = y_axis
    info = dict(z_top=z_top, y_back=y_back, y_axis=y_axis, width=2 * r_max, screws=screws)
    return assembled, print_poses, info


def to_trimesh(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts), process=True)


if __name__ == "__main__":
    assembled, poses, info = build()
    print(info)
    os.makedirs(os.path.join(OUT, "stl"), exist_ok=True)
    for name, m in assembled.items():
        tm = to_trimesh(m)
        print(f"{name:16s} tris={len(tm.faces):7d} watertight={tm.is_watertight} vol={tm.volume / 1000:6.1f} cm3 "
              f"bbox={np.round(tm.extents, 1)}")
    # body + inlays share one offset so they stay registered as one multi-colour object
    body = to_trimesh(poses["body_blue"])
    body_shift = [-body.bounds[0][0] - body.extents[0] / 2, -body.bounds[0][1] - body.extents[1] / 2, -body.bounds[0][2]]
    for name, m in poses.items():
        tm = to_trimesh(m)
        if name in ("body_blue", "belly_cream", "accents_red"):
            tm.apply_translation(body_shift)
        else:
            tm.apply_translation([-tm.bounds[0][0] - tm.extents[0] / 2, -tm.bounds[0][1] - tm.extents[1] / 2, -tm.bounds[0][2]])
        tm.export(os.path.join(OUT, "stl", f"goli_{name}.stl"))
    os.makedirs(os.path.join(OUT, "stl", "assembly"), exist_ok=True)
    for name, m in assembled.items():
        to_trimesh(m).export(os.path.join(OUT, "stl", "assembly", f"{name}.stl"))
