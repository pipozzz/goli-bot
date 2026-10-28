"""Bundle ./stl/assembly into a self-contained three.js viewer (viewer.html)."""
import base64
import json
import math
import os
import sys

import numpy as np
import trimesh

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import goli_bot  # noqa: E402

PARTS = {
    # name: (colour, roughness, explode direction in model coords)
    "body_blue": ("#6c7ae0", 0.55, (0, 0, 0)),
    "belly_cream": ("#f3e6cc", 0.6, (0, 18, 0)),
    "accents_red": ("#e5484d", 0.5, (0, 22, 0)),
    "bezel_navy": ("#2b2d42", 0.45, (0, 40, 6)),
    "bracket_any": ("#a7adc2", 0.6, (0, -55, 0)),
    "backplate_blue": ("#6c7ae0", 0.55, (0, -75, 0)),
    "bell_yellow": ("#f2b84b", 0.3, (0, 0, 28)),
    "mug_white": ("#fafafa", 0.35, (16, 0, 0)),
}


def b64(a):
    return base64.b64encode(np.ascontiguousarray(a).tobytes()).decode()


def part(name):
    tm = trimesh.load(os.path.join(HERE, "stl", "assembly", f"{name}.stl"))
    tm.merge_vertices()
    sm = tm.smooth_shaded  # splits vertices at sharp edges -> crisp cuts, smooth egg
    v = sm.vertices[:, [0, 2, 1]] * np.array([1, 1, -1])   # z-up -> y-up (rotation)
    n = sm.vertex_normals[:, [0, 2, 1]] * np.array([1, 1, -1])
    return dict(v=b64(v.astype(np.float32)), n=b64(n.astype(np.float32)), i=b64(sm.faces.astype(np.uint32)))


def screen():
    """Disc for the AMOLED (visible area), in the tilted face frame."""
    t = math.radians(goli_bot.FACE_TILT)
    u = np.array([1, 0, 0])
    w = np.array([0, math.cos(t), math.sin(t)])
    vv = np.array([0, -math.sin(t), math.cos(t)])
    o = np.array([0, goli_bot.Y_AXIS, goli_bot.FACE_Z]) - 0.02 * w
    r = goli_bot.VA_R + 0.6
    n = 96
    pts, uvs = [o], [[0.5, 0.5]]
    for k in range(n):
        a = 2 * math.pi * k / n
        pts.append(o + r * (math.cos(a) * u + math.sin(a) * vv))
        uvs.append([0.5 - 0.5 * math.cos(a), 0.5 + 0.5 * math.sin(a)])  # +x is the viewer's left
    faces = [[0, k + 1, (k + 1) % n + 1] for k in range(n)]
    pts = np.array(pts)[:, [0, 2, 1]] * np.array([1, 1, -1])
    nrm = w[[0, 2, 1]] * np.array([1, 1, -1])
    return dict(v=b64(pts.astype(np.float32)), uv=b64(np.array(uvs, np.float32)),
                i=b64(np.array(faces, np.uint32)), n=nrm.tolist())


def main():
    goli_bot.Y_AXIS = goli_bot.solve_face_axis()
    data = {k: dict(part(k), color=c, rough=r, ex=[e[0], e[2], -e[1]]) for k, (c, r, e) in PARTS.items()}
    payload = json.dumps(dict(parts=data, screen=screen()))
    html = open(os.path.join(HERE, "viewer_template.html")).read().replace("/*__DATA__*/null", payload)
    out = os.path.join(HERE, "viewer.html")
    open(out, "w").write(html)
    print("wrote", out, f"{len(html) / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
