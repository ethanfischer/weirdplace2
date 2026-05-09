"""
Creates /Game/Sounds/SC_Ambient (a USoundClass with bApplyAmbientVolumes=true)
and assigns it to /Game/Sounds/Wind.SoundClassObject.

This is the gate that lets AudioVolume interior settings (ExteriorVolume,
ExteriorLPF) actually apply to the wind. Without a SoundClass that opts in,
FActiveSound::HandleInteriorVolumes is never called (engine-side check at
SoundBase.cpp:110: `return SoundClass && SoundClass->Properties.bApplyAmbientVolumes;`).

Idempotent. Run with editor open or headless via UnrealEditor-Cmd.exe.
"""

import unreal


SC_PATH = "/Game/Sounds/SC_Ambient"
SC_NAME = "SC_Ambient"
SC_DIR = "/Game/Sounds"
WIND_PATH = "/Game/Sounds/Wind"


def ensure_sound_class():
    if unreal.EditorAssetLibrary.does_asset_exist(SC_PATH):
        sc = unreal.EditorAssetLibrary.load_asset(SC_PATH)
        unreal.log(f"[sound_class] reusing existing {SC_PATH}")
    else:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        sc = tools.create_asset(SC_NAME, SC_DIR, unreal.SoundClass, None)
        if not sc:
            raise RuntimeError(f"Failed to create {SC_PATH}")
        unreal.log(f"[sound_class] created {SC_PATH}")

    props = sc.get_editor_property("properties")
    props.set_editor_property("apply_ambient_volumes", True)
    sc.set_editor_property("properties", props)
    unreal.EditorAssetLibrary.save_loaded_asset(sc)
    return sc


def assign_to_wind(sc):
    wind = unreal.EditorAssetLibrary.load_asset(WIND_PATH)
    if not wind:
        raise RuntimeError(f"Failed to load {WIND_PATH}")

    current = wind.get_editor_property("sound_class_object")
    if current == sc:
        unreal.log(f"[sound_class] {WIND_PATH} already references {SC_PATH}")
        return

    wind.set_editor_property("sound_class_object", sc)
    unreal.EditorAssetLibrary.save_loaded_asset(wind)
    unreal.log(f"[sound_class] {WIND_PATH}.sound_class_object <- {SC_PATH}")


def main():
    unreal.log("=== audio_ensure_sound_class.py ===")
    sc = ensure_sound_class()
    assign_to_wind(sc)
    unreal.log("=== Done ===")


if __name__ == "__main__":
    main()
