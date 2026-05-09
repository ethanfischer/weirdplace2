"""
Phase 5 of the Soundscape -> AudioVolume migration.

1. Configures every per-room ambient stub (AAmbientSound with label
   'Ambient_<RoomName>', excluding Ambient_GlobalWind): bAutoActivate=true,
   spatialization on, attenuation override on (preserves any radius the user
   already tuned in the level).
2. Deletes the now-defunct Soundscape assets:
       /Game/Sounds/Soundscape/Palette_Outside
       /Game/Sounds/Soundscape/Palette_Inside
       /Game/Sounds/Soundscape/Color_Wind
       /Game/Sounds/Soundscape/Color_WindInside
       /Game/SColor_GreetingBell
3. Saves the level package.

Idempotent: missing assets are skipped, already-configured stubs are no-ops.

Run with editor open:
    py "C:/Users/ethan/repos/weirdplace2/Content/Python/audio_phase5_configure_stubs_and_cleanup.py"
Or headless:
    UnrealEditor-Cmd.exe weirdplace2.uproject \
        -ExecutePythonScript=Content/Python/audio_phase5_configure_stubs_and_cleanup.py \
        -unattended -nopause -nosplash
"""

import unreal


LEVEL_PATH = "/Game/FirstPerson/Maps/FirstPersonMap"
GLOBAL_WIND_LABEL = "Ambient_GlobalWind"
LABEL_PREFIX = "Ambient_"

DEFUNCT_ASSETS = [
    "/Game/Sounds/Soundscape/Palette_Outside",
    "/Game/Sounds/Soundscape/Palette_Inside",
    "/Game/Sounds/Soundscape/Color_Wind",
    "/Game/Sounds/Soundscape/Color_WindInside",
    "/Game/SColor_GreetingBell",
]


def configure_stub(actor):
    audio_comp = actor.get_component_by_class(unreal.AudioComponent)
    if not audio_comp:
        raise RuntimeError(f"{actor.get_actor_label()} has no AudioComponent")

    audio_comp.set_editor_property("auto_activate", True)
    audio_comp.set_editor_property("allow_spatialization", True)
    audio_comp.set_editor_property("override_attenuation", True)


def configure_per_room_stubs():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = eas.get_all_level_actors()

    touched_packages = set()
    matched = 0
    for actor in actors:
        if not isinstance(actor, unreal.AmbientSound):
            continue
        label = actor.get_actor_label()
        if not label.startswith(LABEL_PREFIX):
            continue
        if label == GLOBAL_WIND_LABEL:
            continue
        configure_stub(actor)
        touched_packages.add(actor.get_outer().get_package())
        matched += 1
        unreal.log(f"[audio_phase5] configured AmbientSound '{label}'")

    if matched == 0:
        unreal.log_warning(
            "[audio_phase5] no per-room Ambient_<Room> stubs found — that's fine "
            "if you haven't placed any yet."
        )

    for pkg in touched_packages:
        unreal.EditorLoadingAndSavingUtils.save_packages([pkg], False)
    return matched


def delete_defunct_assets():
    deleted = 0
    for path in DEFUNCT_ASSETS:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log(f"[audio_phase5] already absent: {path}")
            continue
        ok = unreal.EditorAssetLibrary.delete_asset(path)
        if ok:
            unreal.log(f"[audio_phase5] deleted {path}")
            deleted += 1
        else:
            unreal.log_error(f"[audio_phase5] failed to delete {path}")
    return deleted


def main():
    unreal.log("=== audio_phase5_configure_stubs_and_cleanup.py ===")
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    stubs = configure_per_room_stubs()
    deleted = delete_defunct_assets()
    # Save the level once more in case asset deletion dirtied it via references.
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"[audio_phase5] done. stubs configured: {stubs}, assets deleted: {deleted}")


if __name__ == "__main__":
    main()
