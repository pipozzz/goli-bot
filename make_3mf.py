"""Builds stl/goli_body_multicolor.3mf: the body, belly and accents as one object with three
filaments, for the Bambu Lab A1 mini with the AMS lite (open it in Bambu Studio and print).

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


def main():
    if not os.path.exists(STUDIO):
        sys.exit(f"Bambu Studio not found at {STUDIO}; set BAMBU_STUDIO")
    out = os.path.join(HERE, "stl", "goli_body_multicolor.3mf")
    with tempfile.TemporaryDirectory() as tmp:
        process = resolve("process", PROCESS)
        process.update(PROCESS_OVERRIDES)
        process_path = os.path.join(tmp, "process.json")
        with open(process_path, "w") as fh:
            json.dump(process, fh)
        filaments = []
        for i, (_, colour) in enumerate(PARTS):
            f = resolve("filament", FILAMENT)
            f["filament_colour"] = [colour]
            filaments.append(os.path.join(tmp, f"filament{i}.json"))
            with open(filaments[-1], "w") as fh:
                json.dump(f, fh)
        machine = os.path.join(PROFILES, "machine", MACHINE + ".json")
        cmd = [STUDIO, "--assemble", "--arrange", "1",  # one object: arranging keeps the parts registered
               "--load-settings", f"{machine};{process_path}",
               "--load-filaments", ";".join(filaments),
               "--load-filament-ids", ",".join(str(i + 1) for i in range(len(PARTS))),
               "--export-3mf", out] + [os.path.join(HERE, "stl", f) for f, _ in PARTS]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode or not os.path.exists(out):
            sys.exit(f"Bambu Studio failed ({r.returncode}):\n{r.stdout[-2000:]}{r.stderr[-2000:]}")
    print("wrote", os.path.relpath(out, HERE))


if __name__ == "__main__":
    main()
