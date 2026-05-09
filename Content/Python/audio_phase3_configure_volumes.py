"""
Phase 3 of the Soundscape -> AudioVolume migration.

Walks every actor in FirstPersonMap, finds AAudioVolume actors whose label
starts with 'AV_', and applies the project's standard ambient zone settings
so that exterior sounds (Ambient_GlobalWind etc.) duck + low-pass when the
listener is inside the volume.

Idempotent: rerun any time to retune values.

Run with editor open:
    py "C:/Users/ethan/repos/weirdplace2/Content/Python/audio_phase3_configure_volumes.py"
Or headless:
    UnrealEditor-Cmd.exe weirdplace2.uproject \
        -ExecutePythonScript=Content/Python/audio_phase3_configure_volumes.py \
        -unattended -nopause -nosplash
"""

import unreal


LEVEL_PATH = "/Game/FirstPerson/Maps/FirstPersonMap"
LABEL_PREFIX = "AV_"

# LPF values are FREQUENCY CUTOFFS IN HZ, not 0-1 ratios. Default is 20000
# (no muffle). Lower Hz = heavier muffle. Reference points:
#   ~6000 Hz = subtle "next room" muffle
#   ~1500 Hz = clear "behind a wall" muffle
#   ~600  Hz = heavy "underwater / sealed room" muffle
# Volume drop in (0, 1] — 1.0 = no ducking, 0.2 = -14dB.
#
# Per-room overrides keyed on actor label. Listener-progression in this map
# (outside is unmuffled and full volume by default):
#   VideoStore        -> light  (one wall away from outside)
#   VideoStoreHallway -> medium (deeper interior)
#   VideoStoreBathroom-> heavy  (most enclosed)
EXTERIOR_TIME = 1.5
EXTERIOR_LPF_TIME = 1.5
INTERIOR_VOLUME = 1.0
INTERIOR_TIME = 1.5
INTERIOR_LPF = 20000.0
INTERIOR_LPF_TIME = 1.5
PRIORITY = 10.0

# label -> (exterior_volume, exterior_lpf_hz)
OVERRIDES = {
    "AV_VideoStore":         (0.6, 4000.0),
    "AV_VideoStoreHallway":  (0.4, 1500.0),
    "AV_VideoStoreBathroom": (0.2, 600.0),
}
DEFAULT = (0.35, 1500.0)  # used for any AV_* without an explicit entry


def configure_volume(av):
    av.set_editor_property("priority", PRIORITY)
    av.set_editor_property("enabled", True)

    label = av.get_actor_label()
    ext_vol, ext_lpf = OVERRIDES.get(label, DEFAULT)

    zone = av.get_editor_property("ambient_zone_settings")
    zone.set_editor_property("exterior_volume", ext_vol)
    zone.set_editor_property("exterior_time", EXTERIOR_TIME)
    zone.set_editor_property("exterior_lpf", ext_lpf)
    zone.set_editor_property("exterior_lpf_time", EXTERIOR_LPF_TIME)
    zone.set_editor_property("interior_volume", INTERIOR_VOLUME)
    zone.set_editor_property("interior_time", INTERIOR_TIME)
    zone.set_editor_property("interior_lpf", INTERIOR_LPF)
    zone.set_editor_property("interior_lpf_time", INTERIOR_LPF_TIME)
    av.set_editor_property("ambient_zone_settings", zone)
    return ext_vol, ext_lpf


def main():
    unreal.log("=== audio_phase3_configure_volumes.py ===")
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = eas.get_all_level_actors()

    touched_packages = set()
    matched = 0
    for actor in actors:
        if not isinstance(actor, unreal.AudioVolume):
            continue
        label = actor.get_actor_label()
        if not label.startswith(LABEL_PREFIX):
            continue
        ext_vol, ext_lpf = configure_volume(actor)
        touched_packages.add(actor.get_outer().get_package())
        matched += 1
        unreal.log(f"[audio_phase3] {label}: exterior_volume={ext_vol}, exterior_lpf={ext_lpf} Hz")

    if matched == 0:
        unreal.log_warning(
            f"[audio_phase3] no AudioVolume actors with label starting '{LABEL_PREFIX}' "
            "found — make sure you placed + renamed them in the level."
        )
        return

    for pkg in touched_packages:
        unreal.EditorLoadingAndSavingUtils.save_packages([pkg], False)
    unreal.log(f"[audio_phase3] saved {len(touched_packages)} package(s), {matched} volume(s)")


if __name__ == "__main__":
    main()
