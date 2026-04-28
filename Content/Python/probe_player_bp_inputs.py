"""
Inspect BP_FirstPersonCharacter via FBlueprintEditorLibrary helpers + uasset
binary scan to find any leftover legacy input bindings.
"""

import unreal
import os
import struct


BP_PATH = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
UASSET_FS_PATH = unreal.SystemLibrary.get_project_directory() + "Content/FirstPerson/Blueprints/BP_FirstPersonCharacter.uasset"


def list_bp_functions():
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    if not bp:
        unreal.log_error("Could not load BP")
        return
    gc = bp.generated_class()
    if not gc:
        unreal.log_error("No generated class")
        return

    unreal.log(f"=== Class default object inspection ===")
    cdo = unreal.get_default_object(gc)

    # Try BlueprintEditorLibrary
    try:
        funcs = unreal.BlueprintEditorLibrary.generate_blueprint_function_signatures(bp)
        unreal.log(f"Generated function signatures: {len(funcs) if funcs else 0}")
        if funcs:
            for f in funcs:
                unreal.log(f"  fn: {f}")
    except Exception as e:
        unreal.log(f"generate_blueprint_function_signatures unavailable: {e}")


def scan_uasset_for_input_keywords():
    """Brute-force string scan of the uasset for input-related names."""
    if not os.path.exists(UASSET_FS_PATH):
        unreal.log_error(f"uasset not found at {UASSET_FS_PATH}")
        return

    with open(UASSET_FS_PATH, "rb") as f:
        data = f.read()

    keywords = [
        b"Turn Right / Left Mouse",
        b"Turn Right / Left Gamepad",
        b"Look Up / Down Mouse",
        b"Look Up / Down Gamepad",
        b"AddControllerYawInput",
        b"AddControllerPitchInput",
        b"InputAxisEvent",
        b"InputAxis Turn",
        b"InputAxis Look",
        b"InputAction",
        b"K2Node_InputAxis",
        b"K2Node_InputKey",
        b"AddYawInput",
        b"AddPitchInput",
    ]

    unreal.log("=== uasset binary scan ===")
    for kw in keywords:
        # Find all occurrences (case-sensitive ASCII).
        idx = 0
        hits = 0
        while True:
            idx = data.find(kw, idx)
            if idx < 0:
                break
            hits += 1
            idx += len(kw)
        unreal.log(f"  '{kw.decode('ascii')}': {hits} hits")


list_bp_functions()
scan_uasset_for_input_keywords()
