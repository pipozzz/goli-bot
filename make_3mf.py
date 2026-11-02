"""Builds the body's Bambu Studio projects for the A1 mini (open one in Bambu Studio and print):
stl/goli_body_multicolor.3mf, the body, belly and accents as one object with three filaments (AMS lite),
and stl/goli_body_single.3mf, the body alone in one filament (much faster; paint the details after).
Both also hold the other parts: plate 2 the back plate (the body's blue), plate 3 the face bezel and the
display bracket (navy), plate 4 the bell (yellow) and the mug (white).
With --mini: the same two from stl/mini (Goli at 90 %, after python3 goli_bot.py --mini).

Uses Bambu Studio's own command line, so the project opens exactly like one saved from the app.
Run after goli_bot.py:  python3 make_3mf.py   (BAMBU_STUDIO=/path/to/BambuStudio to override)
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
STUDIO = os.environ.get("BAMBU_STUDIO", "/Applications/BambuStudio.app/Contents/MacOS/BambuStudio")
PROFILES = os.path.join(os.path.dirname(os.path.dirname(STUDIO)), "Resources", "profiles", "BBL")

MACHINE = "Bambu Lab A1 mini 0.4 nozzle"
PROCESS = "0.20mm Standard @BBL A1M"
FILAMENT = "Bambu PLA Basic @BBL A1M"
PARTS = [  # file (after the prefix), colour (Goli's palette)
    ("body_blue.stl", "#6C7AE0"),
    ("belly_cream.stl", "#F3E9D2"),
    ("accents_red.stl", "#E5484D"),
]
# The print settings from the README: 3 walls, 15 % gyroid, normal (grid) supports everywhere. The arms
# start as islands beside the body at 2 cm and a tree's thin tip let them go into the air; inside, the
# ceiling by the face needs support too (it comes out through the open back).
PROCESS_OVERRIDES = {
    "wall_loops": "3",
    "sparse_infill_density": "15%",
    "sparse_infill_pattern": "gyroid",
    "enable_support": "1",
    "support_type": "normal(auto)",
    "support_on_build_plate_only": "0",
}


def find_preset(kind, name):
    for root, _, files in os.walk(os.path.join(PROFILES, kind)):
        for f in files:
            if f.endswith(".json"):
                with open(os.path.join(root, f)) as fh:
                    d = json.load(fh)
                if d.get("name") == name:
                    return d
    sys.exit(f"Bambu Studio preset not found: {kind}/{name}")


def resolve(kind, name):
    """A system preset with everything it inherits merged in (the command line wants it complete)."""
    chain = [find_preset(kind, name)]
    while chain[-1].get("inherits"):
        chain.append(find_preset(kind, chain[-1]["inherits"]))
    out = {}
    for d in reversed(chain):
        out.update(d)
    out.pop("inherits", None)
    out["name"] = name  # keep the system name so Bambu Studio links the project to its presets
    return out


BACK_PLATE = "backplate_blue.stl"  # flat, already lying face down; filament 1 (the body's blue)
# The small parts, each lying as it prints, on plates of their own: (plate, [(file, colour)]). They need
# no supports; the bell and mug come out better at 0.12 mm.
SMALL_PLATES = [
    ("Face bezel and bracket", [("bezel_navy.stl", "#2B2D42"), ("bracket_any.stl", "#2B2D42")], {}),
    ("Bell and mug", [("bell_yellow.stl", "#F2B84B"), ("mug_white.stl", "#FAFAFA")], {"layer_height": "0.12"}),
]


def lift_object_settings(path):
    """The command line files print_params under each part, where Bambu Studio ignores supports and layer
    height: move them up to their object."""
    with zipfile.ZipFile(path) as z:
        files = {i: z.read(i) for i in z.namelist()}
        infos = z.infolist()
    cfg = files["Metadata/model_settings.config"].decode()
    keys = "enable_support|layer_height"

    def fix(m):
        obj = m.group(0)
        found = dict(re.findall(rf'<metadata key="({keys})" value="([^"]*)"/>', obj))
        obj = re.sub(rf'\n\s*<metadata key="(?:{keys})" value="[^"]*"/>', "", obj)
        lines = "".join(f'\n    <metadata key="{k}" value="{v}"/>' for k, v in found.items())
        return re.sub(r'(<metadata key="name" value="[^"]*"/>)', lambda n: n.group(1) + lines, obj, count=1)

    files["Metadata/model_settings.config"] = re.sub(r"<object .*?</object>", fix, cfg, flags=re.S).encode()
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for info in infos:
            z.writestr(info, files[info.filename])


def build(name, parts, folder="stl", prefix="goli_"):
    """One Bambu Studio project: plate 1 the body from STL parts [(file, colour)] as one object, one
    filament per part; plate 2 the back plate; then SMALL_PLATES, on filaments after the body's."""
    out = os.path.join(HERE, folder, prefix + name)
    colours = [c for _, c in parts]
    for _, small, _ in SMALL_PLATES:
        for _, c in small:
            if c not in colours:
                colours.append(c)
    with tempfile.TemporaryDirectory() as tmp:
        process = resolve("process", PROCESS)
        process.update(PROCESS_OVERRIDES)
        process_path = os.path.join(tmp, "process.json")
        with open(process_path, "w") as fh:
            json.dump(process, fh)
        filaments = []
        for i, colour in enumerate(colours):
            f = resolve("filament", FILAMENT)
            f["filament_colour"] = [colour]
            filaments.append(os.path.join(tmp, f"filament{i}.json"))
            with open(filaments[-1], "w") as fh:
                json.dump(f, fh)
        # The same assemble_index merges the parts into one object (they stay registered)
        plates = {"plates": [
            {"plate_name": "Body", "need_arrange": True, "objects": [
                {"path": os.path.join(HERE, folder, prefix + f), "count": 1, "filaments": [i + 1], "assemble_index": [1]}
                for i, (f, _) in enumerate(parts)]},
            {"plate_name": "Back plate", "need_arrange": True, "objects": [
                {"path": os.path.join(HERE, folder, prefix + BACK_PLATE), "count": 1, "filaments": [1], "assemble_index": [2],
                 "print_params": {"enable_support": "0"}}]},
        ]}
        index = 3
        for plate_name, small, params in SMALL_PLATES:
            objects = []
            for f, colour in small:
                objects.append({"path": os.path.join(HERE, folder, prefix + f), "count": 1,
                                "filaments": [colours.index(colour) + 1], "assemble_index": [index],
                                "print_params": {"enable_support": "0", **params}})
                index += 1
            plates["plates"].append({"plate_name": plate_name, "need_arrange": True, "objects": objects})
        plates_path = os.path.join(tmp, "plates.json")
        with open(plates_path, "w") as fh:
            json.dump(plates, fh)
        machine = os.path.join(PROFILES, "machine", MACHINE + ".json")
        cmd = [STUDIO, "--load-settings", f"{machine};{process_path}",
               "--load-filaments", ";".join(filaments),
               "--load-assemble-list", plates_path,
               "--export-3mf", out]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode or not os.path.exists(out):
            sys.exit(f"Bambu Studio failed ({r.returncode}):\n{r.stdout[-2000:]}{r.stderr[-2000:]}")
    lift_object_settings(out)
    print("wrote", os.path.relpath(out, HERE))


def main():
    if not os.path.exists(STUDIO):
        sys.exit(f"Bambu Studio not found at {STUDIO}; set BAMBU_STUDIO")
    where = ("stl/mini", "goli_mini_") if "--mini" in sys.argv else ("stl", "goli_")
    build("body_multicolor.3mf", PARTS, *where)  # AMS lite (many filament changes)
    build("body_single.3mf", PARTS[:1], *where)  # one filament; paint the belly and cheeks after


if __name__ == "__main__":
    main()
