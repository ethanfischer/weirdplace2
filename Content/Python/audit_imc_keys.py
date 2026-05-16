import unreal

for path in ['/Game/FirstPerson/Input/IMC_Default', '/Game/FirstPerson/Input/IMC_Weapons']:
    imc = unreal.load_asset(path)
    print('===', path)
    if imc is None:
        print('  (not found)')
        continue
    for m in imc.mappings:
        action_name = m.action.get_name() if m.action else '<none>'
        try:
            key_str = m.key.get_editor_property('key_name')
        except Exception:
            key_str = unreal.SystemLibrary.get_key_display_name(m.key)
        print(f'  {action_name:<25} <- {key_str}')
