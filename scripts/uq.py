"""
uq — query and command the live UE editor from the terminal.

One CLI for the things that used to be one-off scripts/local/*.py files.
Talks to the running editor via Python Remote Execution (scripts/ue_remote_exec.py).

    python scripts/uq.py actors Seneca              # find actors by label/name substring
    python scripts/uq.py actors --cls Door          # filter by class substring
    python scripts/uq.py sel                        # selected actors
    python scripts/uq.py components BP_Rick2        # component tree of an actor
    python scripts/uq.py props BP_Rick2             # all editor properties
    python scripts/uq.py props BP_Rick2/Body --match material
    python scripts/uq.py props /Game/Blueprints/BP_TV          # asset properties
    python scripts/uq.py props cdo:/Game/Blueprints/BP_TV      # Blueprint CDO properties
    python scripts/uq.py get CarRide SceneryRoot
    python scripts/uq.py set PointLight_3 intensity 5000
    python scripts/uq.py bounds MovieShelf          # NOTE: stale right after set_static_mesh
    python scripts/uq.py screenshot --name look.png --res 1920x1080
    python scripts/uq.py mat-params /Game/CreatedMaterials/VHSCoverMaterials/MI_VHSCover_X
    python scripts/uq.py assets /Game/Dialogue --cls SoundWave
    python scripts/uq.py refs /Game/Blueprints/BP_TV --deps
    python scripts/uq.py cvar wp.CarRideSpeed       # read (prints value + help from log)
    python scripts/uq.py cvar wp.CarRideSpeed 900   # set
    python scripts/uq.py cvar --dump                # wp.Tunables: all wp.* + modified markers
    python scripts/uq.py exec "stat fps"
    python scripts/uq.py py "print(unreal.get_default_object(unreal.GameplayStatics))"
    python scripts/uq.py pyfile C:/abs/path/script.py
    python scripts/uq.py save                       # save dirty packages

Add --json to any query verb for machine-readable output.
"""

import argparse
import json
import sys
import time
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SCRIPTS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPTS_DIR.parent
PAYLOAD_PATH = PROJECT_ROOT / "Saved/uq/_payload.py"
EDITOR_LOG = PROJECT_ROOT / "Saved/Logs/weirdplace2.log"
SCREENSHOT_DIRS = [
    PROJECT_ROOT / "Saved/Screenshots/WindowsEditor",
    PROJECT_ROOT / "Saved/Screenshots/Windows",
    PROJECT_ROOT / "Saved/Screenshots",
]

sys.path.insert(0, str(SCRIPTS_DIR))
from ue_remote_exec import run_code, EditorNotFound  # noqa: E402


def fix_game_path(s):
    """Undo Git Bash (MSYS) path mangling: '/Game/X' arrives as
    'C:/Program Files/Git/Game/X'. Also accept 'Game/X' without the slash."""
    if s is None:
        return s
    for root in ("/Game/", "/Engine/", "/Script/"):
        i = s.find(root)
        if i > 0 and ":/" in s[:i]:
            return s[i:]
    if s.startswith(("Game/", "Engine/", "Script/")):
        return "/" + s
    return s

# ---------------------------------------------------------------------------
# Payload prelude shipped to the editor with every verb. Keep it dependency-free.
# ---------------------------------------------------------------------------
PRELUDE = r'''
import unreal, json, traceback

def _emit(obj):
    print("UQ_JSON_BEGIN")
    print(json.dumps(obj, default=str))
    print("UQ_JSON_END")

def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

def _game_world():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    except Exception:
        return None

def _all_actors():
    return list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())

def _label(a):
    try:
        return a.get_actor_label()
    except Exception:
        return a.get_name()

def _find_actor(query):
    q = query.lower()
    actors = _all_actors()
    exact = [a for a in actors if _label(a).lower() == q or a.get_name().lower() == q]
    if len(exact) == 1:
        return exact[0]
    subs = [a for a in actors if q in _label(a).lower() or q in a.get_name().lower()]
    if len(subs) == 1:
        return subs[0]
    if not subs:
        raise LookupError("no actor matching '%s'" % query)
    names = ", ".join(sorted(_label(a) for a in subs)[:20])
    raise LookupError("ambiguous '%s' (%d matches): %s" % (query, len(subs), names))

def _resolve(spec):
    """'/Game/...' asset | 'cdo:/Game/...' Blueprint CDO | 'Label' | 'Label/Component'"""
    if spec.startswith("cdo:"):
        path = spec[4:]
        asset = unreal.load_asset(path)
        if asset is None:
            raise LookupError("no asset at " + path)
        klass = asset.generated_class() if isinstance(asset, unreal.Blueprint) else asset
        return unreal.get_default_object(klass)
    if spec.startswith("/"):
        asset = unreal.load_asset(spec)
        if asset is None:
            raise LookupError("no asset at " + spec)
        return asset
    if "/" in spec:
        actor_q, comp_q = spec.split("/", 1)
        actor = _find_actor(actor_q)
        comps = list(actor.get_components_by_class(unreal.ActorComponent))
        exact = [c for c in comps if c.get_name().lower() == comp_q.lower()]
        if len(exact) == 1:
            return exact[0]
        subs = [c for c in comps if comp_q.lower() in c.get_name().lower()]
        if len(subs) == 1:
            return subs[0]
        raise LookupError("component '%s' on %s not unique/found; components: %s"
                          % (comp_q, _label(actor), [c.get_name() for c in comps]))
    return _find_actor(spec)

def _props(obj, match=None):
    out = {}
    for name in dir(obj):
        if name.startswith("_"):
            continue
        if match and match.lower() not in name.lower():
            continue
        try:
            val = obj.get_editor_property(name)
        except Exception:
            continue  # not a reflected property (methods etc.)
        r = repr(val)
        if len(r) > 300:
            r = r[:300] + " ..."
        out[name] = r
    return out

def _coerce(old, raw):
    """Coerce CLI string `raw` to the type of current value `old`."""
    if isinstance(old, bool):
        return raw.lower() in ("1", "true", "yes", "on")
    if isinstance(old, int):
        return int(raw)
    if isinstance(old, float):
        return float(raw)
    if isinstance(old, unreal.Name):
        return unreal.Name(raw)
    if isinstance(old, unreal.Text):
        return unreal.Text(raw)
    if isinstance(old, str):
        return raw
    if isinstance(old, unreal.EnumBase):
        key = raw.upper()
        if not hasattr(type(old), key):
            raise ValueError("enum %s has no member %s" % (type(old).__name__, key))
        return getattr(type(old), key)
    if isinstance(old, (unreal.Vector, unreal.Rotator, unreal.LinearColor)):
        try:
            nums = json.loads(raw)
        except Exception:
            nums = [float(x) for x in raw.split(",")]
        if isinstance(old, unreal.Vector):
            return unreal.Vector(*nums)
        if isinstance(old, unreal.Rotator):
            return unreal.Rotator(*nums)  # roll, pitch, yaw
        return unreal.LinearColor(*nums)
    raise ValueError("don't know how to coerce '%s' to %s" % (raw, type(old).__name__))

try:
__UQ_BODY__
except Exception:
    _emit({"error": traceback.format_exc()})
'''

VERB_BODIES = {
    "actors": r'''
    res = []
    for a in _all_actors():
        lbl, nm, cls = _label(a), a.get_name(), a.get_class().get_name()
        if P["pattern"] and P["pattern"].lower() not in lbl.lower() and P["pattern"].lower() not in nm.lower():
            continue
        if P["cls"] and P["cls"].lower() not in cls.lower():
            continue
        loc = None
        try:
            v = a.get_actor_location()
            loc = [round(v.x, 1), round(v.y, 1), round(v.z, 1)]
        except Exception:
            pass
        res.append({"label": lbl, "class": cls, "loc": loc})
    _emit({"actors": res})
''',
    "sel": r'''
    res = []
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_selected_level_actors():
        v = a.get_actor_location()
        res.append({"label": _label(a), "class": a.get_class().get_name(),
                    "loc": [round(v.x, 1), round(v.y, 1), round(v.z, 1)]})
    _emit({"actors": res})
''',
    "components": r'''
    a = _find_actor(P["target"])
    comps = []
    for c in a.get_components_by_class(unreal.ActorComponent):
        parent = None
        try:
            ap = c.get_attach_parent()
            parent = ap.get_name() if ap else None
        except Exception:
            pass
        comps.append({"name": c.get_name(), "class": c.get_class().get_name(), "parent": parent})
    _emit({"actor": _label(a), "components": comps})
''',
    "props": r'''
    obj = _resolve(P["target"])
    _emit({"object": obj.get_full_name(), "props": _props(obj, P["match"])})
''',
    "get": r'''
    obj = _resolve(P["target"])
    val = obj.get_editor_property(P["prop"])
    _emit({"object": obj.get_full_name(), "prop": P["prop"],
           "type": type(val).__name__, "value": repr(val)})
''',
    "set": r'''
    obj = _resolve(P["target"])
    old = obj.get_editor_property(P["prop"])
    new = _coerce(old, P["value"])
    try:
        obj.modify(True)  # OFPA actors don't save without modify()
    except Exception:
        pass
    obj.set_editor_property(P["prop"], new)
    after = obj.get_editor_property(P["prop"])
    _emit({"object": obj.get_full_name(), "prop": P["prop"],
           "old": repr(old), "new": repr(after),
           "note": "in-memory only; run 'uq save' to persist to disk"})
''',
    "bounds": r'''
    a = _find_actor(P["target"])
    origin, extent = a.get_actor_bounds(False)
    _emit({"actor": _label(a),
           "origin": [round(origin.x, 1), round(origin.y, 1), round(origin.z, 1)],
           "extent": [round(extent.x, 1), round(extent.y, 1), round(extent.z, 1)]})
''',
    "screenshot": r'''
    unreal.AutomationLibrary.take_high_res_screenshot(P["w"], P["h"], P["name"])
    _emit({"queued": P["name"]})
''',
    "mat-params": r'''
    mi = unreal.load_asset(P["path"])
    if mi is None:
        raise LookupError("no asset at " + P["path"])
    data = {"asset": mi.get_path_name(), "class": mi.get_class().get_name()}
    if isinstance(mi, unreal.MaterialInstance):
        for field, key in (("scalar_parameter_values", "scalar_overrides"),
                           ("vector_parameter_values", "vector_overrides"),
                           ("texture_parameter_values", "texture_overrides")):
            vals = {}
            try:
                arr = mi.get_editor_property(field)
            except Exception:
                arr = []
            for pv in arr:
                info = pv.get_editor_property("parameter_info")
                nm = str(info.get_editor_property("name"))
                # repr the name: Fab packs ship params like 'Albedo ' (trailing space)
                vals[repr(nm)] = repr(pv.get_editor_property("parameter_value"))[:200]
            data[key] = vals
        parent = mi.get_editor_property("parent")
        data["parent"] = parent.get_path_name() if parent else None
        base = parent
        while isinstance(base, unreal.MaterialInstance):
            base = base.get_editor_property("parent")
        if isinstance(base, unreal.Material):
            mel = unreal.MaterialEditingLibrary
            data["base_scalar_params"] = [repr(str(n)) for n in mel.get_scalar_parameter_names(base)]
            data["base_vector_params"] = [repr(str(n)) for n in mel.get_vector_parameter_names(base)]
            data["base_texture_params"] = [repr(str(n)) for n in mel.get_texture_parameter_names(base)]
    _emit(data)
''',
    "assets": r'''
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    found = ar.get_assets_by_path(P["path"], recursive=True) or []
    res = []
    for ad in found:
        cls = str(ad.asset_class_path.asset_name)
        if P["cls"] and P["cls"].lower() not in cls.lower():
            continue
        res.append({"name": str(ad.asset_name), "class": cls, "package": str(ad.package_name)})
    _emit({"assets": res})
''',
    "refs": r'''
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    pkg = P["path"].split(".")[0]
    opts = unreal.AssetRegistryDependencyOptions()
    key = "dependencies" if P["deps"] else "referencers"
    fn = ar.get_dependencies if P["deps"] else ar.get_referencers
    _emit({"package": pkg, key: sorted(str(p) for p in (fn(pkg, opts) or []))})
''',
    "exec": r'''
    w = _game_world() or _world()
    unreal.SystemLibrary.execute_console_command(w, P["cmd"])
    _emit({"executed": P["cmd"], "world": w.get_name()})
''',
    "save": r'''
    ok = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    _emit({"saved": bool(ok)})
''',
}


def run_verb(verb, params):
    body = VERB_BODIES[verb]
    # PRELUDE ends with a try: block; VERB_BODIES are indented 4 spaces to sit inside it.
    # .replace() because the prelude itself contains literal % format strings.
    code = PRELUDE.replace("__UQ_BODY__", "    P = json.loads(%r)\n%s" % (json.dumps(params), body))
    PAYLOAD_PATH.parent.mkdir(parents=True, exist_ok=True)
    PAYLOAD_PATH.write_text(code, encoding="utf-8")
    success, text = run_code(str(PAYLOAD_PATH).replace("\\", "/"), mode="ExecuteFile")
    if "UQ_JSON_BEGIN" not in text:
        print(text.strip() or "(no output from editor)", file=sys.stderr)
        sys.exit(1)
    blob = text.split("UQ_JSON_BEGIN", 1)[1].split("UQ_JSON_END", 1)[0]
    data = json.loads(blob)
    if "error" in data:
        print(data["error"], file=sys.stderr)
        sys.exit(1)
    return data


def log_offset():
    return EDITOR_LOG.stat().st_size if EDITOR_LOG.exists() else 0


def new_log_lines(offset, settle=0.6):
    time.sleep(settle)
    if not EDITOR_LOG.exists():
        return []
    with open(EDITOR_LOG, "r", encoding="utf-8", errors="replace") as f:
        f.seek(offset)
        return [l.rstrip("\n") for l in f]


def strip_ts(line):
    import re
    return re.sub(r"^\[[\d.:-]+\]\[\s*\d+\]", "", line).strip()


def print_table(rows, columns):
    if not rows:
        print("(none)")
        return
    widths = [max(len(str(r.get(c, ""))) for r in rows + [{c: c}]) for c in columns]
    print("  ".join(c.ljust(w) for c, w in zip(columns, widths)))
    for r in rows:
        print("  ".join(str(r.get(c, "")).ljust(w) for c, w in zip(columns, widths)))


def main():
    ap = argparse.ArgumentParser(
        prog="uq", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--json", action="store_true", help="raw JSON output")
    sub = ap.add_subparsers(dest="verb", required=True)

    p = sub.add_parser("actors", help="list level actors")
    p.add_argument("pattern", nargs="?", default="")
    p.add_argument("--cls", default="")
    sub.add_parser("sel", help="selected actors")
    p = sub.add_parser("components", help="component tree of an actor")
    p.add_argument("target")
    p = sub.add_parser("props", help="dump editor properties of actor/component/asset/CDO")
    p.add_argument("target")
    p.add_argument("--match", default=None)
    p = sub.add_parser("get", help="get one property")
    p.add_argument("target")
    p.add_argument("prop")
    p = sub.add_parser("set", help="set one property (coerced to current type)")
    p.add_argument("target")
    p.add_argument("prop")
    p.add_argument("value")
    p = sub.add_parser("bounds", help="actor bounds (stale right after set_static_mesh!)")
    p.add_argument("target")
    p = sub.add_parser("screenshot", help="editor-viewport screenshot")
    p.add_argument("--name", default="uq_screenshot.png")
    p.add_argument("--res", default="1280x720")
    p = sub.add_parser("mat-params", help="material instance parameter overrides + base params")
    p.add_argument("path")
    p = sub.add_parser("assets", help="list assets under a /Game path")
    p.add_argument("path")
    p.add_argument("--cls", default="")
    p = sub.add_parser("refs", help="asset referencers (--deps for dependencies)")
    p.add_argument("path")
    p.add_argument("--deps", action="store_true")
    p = sub.add_parser("cvar", help="get/set a console variable, or --dump wp.* tunables")
    p.add_argument("name", nargs="?", default=None)
    p.add_argument("value", nargs="?", default=None)
    p.add_argument("--dump", action="store_true", help="run wp.Tunables and show all wp.* cvars")
    p = sub.add_parser("exec", help="run a console command (game world if PIE active)")
    p.add_argument("cmd")
    p = sub.add_parser("py", help="run inline python in the editor")
    p.add_argument("code")
    p = sub.add_parser("pyfile", help="run a python file (absolute path) in the editor")
    p.add_argument("path")
    sub.add_parser("save", help="save dirty packages")
    args = ap.parse_args()

    try:
        dispatch(args)
    except EditorNotFound as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(2)


def dispatch(args):
    verb = args.verb
    for attr in ("path", "target"):
        if hasattr(args, attr):
            setattr(args, attr, fix_game_path(getattr(args, attr)))

    if verb == "py":
        success, text = run_code(args.code, mode="ExecuteStatement")
        print(text.strip())
        sys.exit(0 if success else 1)

    if verb == "pyfile":
        path = str(Path(args.path).resolve()).replace("\\", "/")
        success, text = run_code(path, mode="ExecuteFile")
        print(text.strip())
        sys.exit(0 if success else 1)

    if verb == "cvar":
        if args.dump:
            offset = log_offset()
            run_verb("exec", {"cmd": "weird.Tunables"})
            lines = [strip_ts(l) for l in new_log_lines(offset)]
            hits = [l for l in lines if "weird.Tunables:" in l]
            print("\n".join(hits) if hits else "(no weird.Tunables output — editor restart needed after adding the command?)")
            return
        if args.name is None:
            sys.exit("ERROR: cvar needs a NAME (or --dump)")
        if args.value is not None:
            run_verb("exec", {"cmd": f"{args.name} {args.value}"})
        # Reading: exec the bare cvar name; the engine prints value + help to the log.
        offset = log_offset()
        run_verb("exec", {"cmd": args.name})
        lines = [strip_ts(l) for l in new_log_lines(offset)]
        hits = [l for l in lines
                if args.name.lower() in l.lower()
                and "UQ_JSON" not in l and '"executed"' not in l]
        print("\n".join(hits) if hits else f"(no log output for {args.name} — unknown cvar?)")
        return

    if verb == "screenshot":
        w, h = (int(x) for x in args.res.lower().split("x"))
        name = args.name if args.name.endswith(".png") else args.name + ".png"
        start = time.time()
        run_verb("screenshot", {"w": w, "h": h, "name": name})
        # The shot is queued; the file appears once the frame renders.
        deadline = time.time() + 15
        while time.time() < deadline:
            for d in SCREENSHOT_DIRS:
                f = d / name
                if f.exists() and f.stat().st_mtime >= start - 1 and f.stat().st_size > 0:
                    time.sleep(0.3)  # let the write finish
                    print(f)
                    return
            time.sleep(0.4)
        sys.exit(f"ERROR: screenshot '{name}' did not appear within 15s "
                 f"(is a viewport visible and rendering?)")

    params_by_verb = {
        "actors": lambda: {"pattern": args.pattern, "cls": args.cls},
        "sel": lambda: {},
        "components": lambda: {"target": args.target},
        "props": lambda: {"target": args.target, "match": args.match},
        "get": lambda: {"target": args.target, "prop": args.prop},
        "set": lambda: {"target": args.target, "prop": args.prop, "value": args.value},
        "bounds": lambda: {"target": args.target},
        "mat-params": lambda: {"path": args.path},
        "assets": lambda: {"path": args.path, "cls": args.cls},
        "refs": lambda: {"path": args.path, "deps": args.deps},
        "exec": lambda: {"cmd": args.cmd},
        "save": lambda: {},
    }
    data = run_verb(verb, params_by_verb[verb]())

    if args.json:
        print(json.dumps(data, indent=2))
        return

    if verb in ("actors", "sel"):
        rows = [{"label": a["label"], "class": a["class"],
                 "loc": ",".join(map(str, a["loc"])) if a["loc"] else ""} for a in data["actors"]]
        print_table(rows, ["label", "class", "loc"])
        print(f"({len(rows)} actor(s))")
    elif verb == "components":
        print(f"{data['actor']}:")
        print_table(data["components"], ["name", "class", "parent"])
    elif verb == "props":
        print(data["object"])
        for k in sorted(data["props"]):
            print(f"  {k} = {data['props'][k]}")
        print(f"({len(data['props'])} properties)")
    elif verb == "get":
        print(f"{data['prop']} ({data['type']}) = {data['value']}")
    elif verb == "set":
        print(f"{data['prop']}: {data['old']} -> {data['new']}")
        print(f"note: {data['note']}")
    elif verb == "bounds":
        print(f"{data['actor']}: origin={data['origin']} extent={data['extent']}")
    elif verb == "mat-params":
        for k, v in data.items():
            if isinstance(v, dict):
                print(f"{k}:")
                for name, val in v.items():
                    print(f"  {name} = {val}")
            elif isinstance(v, list):
                print(f"{k}: {', '.join(v) if v else '(none)'}")
            else:
                print(f"{k}: {v}")
    elif verb == "assets":
        print_table(data["assets"], ["name", "class", "package"])
        print(f"({len(data['assets'])} asset(s))")
    elif verb == "refs":
        key = "dependencies" if args.deps else "referencers"
        print(f"{data['package']} {key}:")
        for p in data[key]:
            print(f"  {p}")
        print(f"({len(data[key])})")
    else:
        print(json.dumps(data, indent=2))


if __name__ == "__main__":
    main()
