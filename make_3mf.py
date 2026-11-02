"""Builds the body's Bambu Studio projects for the A1 mini (open one in Bambu Studio and print):
stl/goli_body_multicolor.3mf, the body, belly and accents as one object with three filaments (AMS lite),
and stl/goli_body_single.3mf, the body alone in one filament (much faster; paint the details after).
Both have the back plate on a second plate, in the body's blue.

Uses Bambu Studio's own command line, so the project opens exactly like one saved from the app.
Run after goli_bot.py:  python3 make_3mf.py   (BAMBU_STUDIO=/path/to/BambuStudio to override)
"""
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
STUDIO = os.environ.get("BAMBU_STUDIO", "/Applications/BambuStudio.app/Contents/MacOS/BambuStudio")
PROFILES = os.path.join(os.path.dirname(os.path.dirname(STUDIO)), "Resources", "profiles", "BBL")

MACHINE = "Bambu Lab A1 mini 0.4 nozzle"
PROCESS = "0.20mm Standard @BBL A1M"
FILAMENT = "Bambu PLA Basic @BBL A1M"
PARTS = [  # file, colour (Goli's palette)
    ("goli_body_blue.stl", "#6C7AE0"),
    ("goli_belly_cream.stl", "#F3E9D2"),
    ("goli_accents_red.stl", "#E5484D"),
]
# The print settings from the README: 3 walls, 15 % gyroid, tree supports from the plate (arm undersides)
PROCESS_OVERRIDES = {
    "wall_loops": "3",
    "sparse_infill_density": "15%",
    "sparse_infill_pattern": "gyroid",
    "enable_support": "1",
    "support_type": "tree(auto)",
    "support_on_build_plate_only": "1",
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


BACK_PLATE = "goli_backplate_blue.stl"  # flat, already lying face down; filament 1 (the body's blue)


def build(name, parts):
    """One Bambu Studio project in stl/: plate 1 the body from STL parts [(file, colour)] as one object,
    one filament per part; plate 2 the back plate."""
    out = os.path.join(HERE, "stl", name)
    with tempfile.TemporaryDirectory() as tmp:
        process = resolve("process", PROCESS)
        process.update(PROCESS_OVERRIDES)
        process_path = os.path.join(tmp, "process.json")
        with open(process_path, "w") as fh:
            json.dump(process, fh)
        filaments = []
        for i, (_, colour) in enumerate(parts):
            f = resolve("filament", FILAMENT)
            f["filament_colour"] = [colour]
            filaments.append(os.path.join(tmp, f"filament{i}.json"))
            with open(filaments[-1], "w") as fh:
                json.dump(f, fh)
        # The same assemble_index merges the parts into one object (they stay registered)
        plates = {"plates": [
            {"plate_name": "Body", "need_arrange": True, "objects": [
                {"path": os.path.join(HERE, "stl", f), "count": 1, "filaments": [i + 1], "assemble_index": [1]}
                for i, (f, _) in enumerate(parts)]},
            {"plate_name": "Back plate", "need_arrange": True, "objects": [
                {"path": os.path.join(HERE, "stl", BACK_PLATE), "count": 1, "filaments": [1], "assemble_index": [2]}]},
        ]}
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
    print("wrote", os.path.relpath(out, HERE))


def main():
    if not os.path.exists(STUDIO):
        sys.exit(f"Bambu Studio not found at {STUDIO}; set BAMBU_STUDIO")
    build("goli_body_multicolor.3mf", PARTS)  # AMS lite: about 17 h (280 filament changes)
    build("goli_body_single.3mf", PARTS[:1])  # one filament: about 7 h; paint the belly and cheeks after


if __name__ == "__main__":
    main()
