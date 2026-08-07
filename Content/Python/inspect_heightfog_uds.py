import unreal, traceback

try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        cls = a.get_class().get_name()
        label = a.get_actor_label()
        if isinstance(a, unreal.ExponentialHeightFog):
            c = a.get_editor_property("component")
            vals = {}
            for prop in ("fog_density", "volumetric_fog", "fog_height_falloff", "fog_max_opacity", "start_distance"):
                try:
                    vals[prop] = c.get_editor_property(prop)
                except Exception as e:
                    vals[prop] = f"ERR {e}"
            print(f"FOG | {label} | {vals} | comp_visible={c.is_visible()}")
        if "Ultra_Dynamic_Sky" in cls or "UltraDynamicSky" in cls:
            print(f"UDS ACTOR | {label} | {cls}")
            for prop in ("fog_mode", "volumetric_fog", "fog"):
                try:
                    print(f"  UDS PROP {prop} = {a.get_editor_property(prop)}")
                except Exception as e:
                    print(f"  UDS PROP {prop} unavailable")
        if "Ultra_Dynamic_Weather" in cls:
            print(f"UDW ACTOR | {label} | {cls}")
    print("FOG RECON DONE")
except Exception:
    print("RECON EXCEPTION:")
    traceback.print_exc()
