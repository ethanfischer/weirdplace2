import unreal

imc_paths = [
    "/Game/FirstPerson/Input/IMC_Default",
    "/Game/FirstPerson/Input/IMC_Weapons",
    "/Game/ThirdPerson/Input/IMC_Default",
]

for path in imc_paths:
    asset = unreal.load_asset(path)
    if not asset:
        print("NOT FOUND: " + path)
        continue
    print("\n=== " + path + " ===")
    try:
        mappings = asset.get_editor_property("mappings")
    except Exception as e:
        print("  no mappings prop: " + str(e))
        continue
    for m in mappings:
        try:
            action = m.get_editor_property("action")
            key = m.get_editor_property("key")
            action_name = action.get_name() if action else "<none>"
            try:
                key_name = str(key.key_name)
            except Exception:
                key_name = str(key.get_editor_property("key_name"))
            print("  " + key_name + " -> " + action_name)
        except Exception as e:
            print("  err: " + str(e))
