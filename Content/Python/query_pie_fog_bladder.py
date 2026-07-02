import unreal, traceback

try:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    print("WORLD:", world.get_name() if world else "NO GAME WORLD")

    for f in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.ExponentialHeightFog):
        c = f.get_editor_property("component")
        vals = {}
        for p in ("fog_density", "fog_height_falloff", "fog_max_opacity", "start_distance"):
            try:
                vals[p] = round(float(c.get_editor_property(p)), 5)
            except Exception:
                vals[p] = "ERR"
        print(f"PIE FOG | {vals} | comp_visible={c.is_visible()} | actor_hidden={f.get_editor_property('hidden')}")

    udscls = unreal.load_class(None, "/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky.Ultra_Dynamic_Sky_C")
    for u in unreal.GameplayStatics.get_all_actors_of_class(world, udscls):
        for p in ("fog", "fog_multiplier", "volumetric_fog"):
            try:
                print(f"PIE UDS {p} = {u.get_editor_property(p)}")
            except Exception:
                pass

    pc = unreal.GameplayStatics.get_player_character(world, 0)
    cam = pc.get_component_by_class(unreal.CameraComponent)
    pps = cam.get_editor_property("post_process_settings")
    arr = pps.get_editor_property("weighted_blendables").get_editor_property("array")
    print("BLENDABLES:", [(x.get_editor_property("object").get_name() if x.get_editor_property("object") else None,
                           x.get_editor_property("weight")) for x in arr])
except Exception:
    traceback.print_exc()
print("PIE QUERY DONE")
