import unreal

IMC_PATH = "/Game/FirstPerson/Input/IMC_Default"
imc = unreal.load_asset(IMC_PATH)
if not imc:
    raise RuntimeError(f"IMC not found: {IMC_PATH}")

left_action = unreal.load_asset("/Game/FirstPerson/Input/Actions/IA_NavigateLeft")
right_action = unreal.load_asset("/Game/FirstPerson/Input/Actions/IA_NavigateRight")
if not left_action or not right_action:
    raise RuntimeError("Could not load IA_NavigateLeft / IA_NavigateRight")

# (action, key_name) — mirrors how IA_NextOption is wired (DPad + arrow + letter + stick).
desired = [
    (left_action,  "Gamepad_DPad_Left"),
    (left_action,  "A"),
    (left_action,  "Left"),
    (left_action,  "Gamepad_LeftStick_Left"),
    (right_action, "Gamepad_DPad_Right"),
    (right_action, "D"),
    (right_action, "Right"),
    (right_action, "Gamepad_LeftStick_Right"),
]

def make_key(name):
    k = unreal.Key()
    k.import_text(name)
    return k

mappings = list(imc.get_editor_property("mappings"))

def already_mapped(action, key_name):
    for m in mappings:
        a = m.get_editor_property("action")
        k = m.get_editor_property("key")
        if a == action and k.export_text() == key_name:
            return True
    return False

added = 0
for action, key_name in desired:
    if already_mapped(action, key_name):
        print(f"SKIP (already mapped): {key_name} -> {action.get_name()}")
        continue
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", make_key(key_name))
    mappings.append(mapping)
    added += 1
    print(f"ADD: {key_name} -> {action.get_name()}")

if added > 0:
    imc.set_editor_property("mappings", mappings)
    unreal.EditorAssetLibrary.save_asset(IMC_PATH)
    print(f"Saved {IMC_PATH} (+{added} mappings)")
else:
    print("no changes")
