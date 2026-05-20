import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
target_dir = "/Game/FirstPerson/Input/Actions"

names = ["IA_NavigateLeft", "IA_NavigateRight"]

for name in names:
    asset_path = f"{target_dir}/{name}"
    existing = unreal.load_asset(asset_path)
    if existing:
        print(f"EXISTS: {asset_path}")
        # Ensure value type is BOOLEAN even if pre-existing.
        existing.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        unreal.EditorAssetLibrary.save_asset(asset_path)
        continue

    factory = unreal.InputAction_Factory()
    new_asset = asset_tools.create_asset(name, target_dir, unreal.InputAction, factory)
    if not new_asset:
        print(f"FAILED to create {asset_path}")
        continue

    new_asset.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    unreal.EditorAssetLibrary.save_asset(asset_path)
    print(f"CREATED: {asset_path}")

print("done")
