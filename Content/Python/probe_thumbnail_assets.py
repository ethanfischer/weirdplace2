"""Find known waypoints / player start in the FirstPersonMap to use as a booth location."""
import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/FirstPerson/Maps/FirstPersonMap")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = eas.get_all_level_actors()
for actor in all_actors:
    cls = actor.get_class().get_name()
    label = actor.get_actor_label() or ""
    # Look for player starts, test waypoints, anything tagged as a spawn point
    if cls in ("PlayerStart", "TestWaypoint") or "Approach" in label or "Spawn" in label:
        loc = actor.get_actor_location()
        unreal.log(f"  {cls} '{label}' @ {loc}")
