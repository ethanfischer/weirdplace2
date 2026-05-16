import unreal

IMC_PATH = '/Game/FirstPerson/Input/IMC_Default'

ADDITIONS = [
    ('/Game/FirstPerson/Input/Actions/IA_Interact', 'Gamepad_FaceButton_Left'),
]

imc = unreal.load_asset(IMC_PATH)

mappings = list(imc.mappings)
changed = False

for action_path, key_name in ADDITIONS:
    action = unreal.load_asset(action_path)
    target_key = unreal.Key()
    target_key.set_editor_property('key_name', key_name)
    already = any(
        m.action == action and str(m.key.get_editor_property('key_name')) == key_name
        for m in mappings
    )
    if already:
        print(f'SKIP  {action.get_name()} <- {key_name} (exists)')
        continue
    new_mapping = unreal.EnhancedActionKeyMapping()
    new_mapping.set_editor_property('action', action)
    new_mapping.set_editor_property('key', target_key)
    mappings.append(new_mapping)
    changed = True
    print(f'ADD   {action.get_name()} <- {key_name}')

if changed:
    imc.set_editor_property('mappings', mappings)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(imc, only_if_is_dirty=False)
    print(f'SAVED {IMC_PATH} -> {saved}')
else:
    print('NO CHANGES')
