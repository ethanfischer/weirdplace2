"""
Phase 1 of the Soundscape -> AudioVolume migration.

Idempotent: rerunnable, only spawns Ambient_GlobalWind if absent.

Run with editor open:
    py "C:/Users/ethan/repos/weirdplace2/Content/Python/audio_phase1_prep.py"
Or headless:
    UnrealEditor-Cmd.exe weirdplace2.uproject \
        -ExecutePythonScript=Content/Python/audio_phase1_prep.py \
        -unattended -nopause -nosplash

What it does:
  Opens FirstPersonMap, spawns an AAmbientSound labelled 'Ambient_GlobalWind'
  at a high exterior location (outside any interior AudioVolume), assigns
  /Game/Sounds/Wind, configures the AudioComponent so the engine's built-in
  AudioVolume interior settings apply.

The required AudioComponent setup (ActiveSound.cpp:1341 and SoundBase.cpp:110
in UE 5.4):
  * AudioComponent.bAllowSpatialization = true  - cross-zone branch in
    HandleInteriorVolumes is gated behind this. 2D / unspatialized sounds
    are forced into the "same zone as listener" branch and never get
    exterior treatment.
  * Attenuation override.bSpatialize = false    - prevents panning so the
    wind doesn't sound like it's coming from a specific direction.
  * Attenuation override.bAttenuate = false     - prevents distance-based
    gain reduction so volume stays flat regardless of listener distance.

The other half of the gate, SoundClass->bApplyAmbientVolumes, lives on the
Wind.uasset itself and is set by audio_ensure_sound_class.py.

Stripping the [/Script/Soundscape.SoundscapeSettings] block from
Config/DefaultGame.ini is handled separately (file edit, not Python).
"""

import unreal


LEVEL_PATH = "/Game/FirstPerson/Maps/FirstPersonMap"
WIND_SOUND_PATH = "/Game/Sounds/Wind"
GLOBAL_WIND_LABEL = "Ambient_GlobalWind"
GLOBAL_WIND_LOCATION = unreal.Vector(0.0, 0.0, 5000.0)


def find_actor_by_label(actors, label):
    for a in actors:
        if a.get_actor_label() == label:
            return a
    return None


def ensure_global_wind():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = eas.get_all_level_actors()

    existing = find_actor_by_label(actors, GLOBAL_WIND_LABEL)
    if existing:
        unreal.log(f"[audio_phase1] '{GLOBAL_WIND_LABEL}' already present, reusing")
        actor = existing
    else:
        actor = eas.spawn_actor_from_class(
            unreal.AmbientSound, GLOBAL_WIND_LOCATION, unreal.Rotator(0, 0, 0)
        )
        if not actor:
            raise RuntimeError("Failed to spawn AAmbientSound for Ambient_GlobalWind")
        actor.set_actor_label(GLOBAL_WIND_LABEL)
        unreal.log(f"[audio_phase1] spawned '{GLOBAL_WIND_LABEL}' at {GLOBAL_WIND_LOCATION}")

    sound = unreal.EditorAssetLibrary.load_asset(WIND_SOUND_PATH)
    if not sound:
        raise RuntimeError(f"Failed to load wind sound at {WIND_SOUND_PATH}")

    audio_comp = actor.get_component_by_class(unreal.AudioComponent)
    if not audio_comp:
        raise RuntimeError("Ambient_GlobalWind has no AudioComponent")

    audio_comp.set_editor_property("sound", sound)
    audio_comp.set_editor_property("auto_activate", True)
    audio_comp.set_editor_property("volume_multiplier", 1.0)
    audio_comp.set_editor_property("allow_spatialization", True)
    audio_comp.set_editor_property("override_attenuation", True)
    att = audio_comp.get_editor_property("attenuation_overrides")
    att.set_editor_property("attenuate", False)
    att.set_editor_property("spatialize", False)
    audio_comp.set_editor_property("attenuation_overrides", att)

    pkg = actor.get_outer().get_package()
    unreal.EditorLoadingAndSavingUtils.save_packages([pkg], False)
    unreal.log(f"[audio_phase1] saved level package for '{GLOBAL_WIND_LABEL}'")


def main():
    unreal.log("=== audio_phase1_prep.py ===")
    ensure_global_wind()
    unreal.log("=== Done ===")


if __name__ == "__main__":
    main()
