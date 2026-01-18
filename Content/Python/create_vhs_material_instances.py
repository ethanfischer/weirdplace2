"""
VHS Cover Material Instance Generator

Run this script in UE Editor via:
  Window -> Developer Tools -> Output Log
  Then: py "Content/Python/create_vhs_material_instances.py"

Or via Python console in Editor Scripting Utilities.

Prerequisites:
1. Import all VHS cover textures into Content/VHSCovers/
2. Create master material M_VHSCover in Content/CreatedMaterials/
   - Add TextureSampleParameter2D named "CoverTexture" connected to Base Color
"""

import unreal

def create_vhs_material_instances():
    # Configuration
    textures_path = '/Game/VHSCovers'
    materials_output_path = '/Game/VHSCovers/Materials'
    master_material_path = '/Game/CreatedMaterials/M_VHSCover'
    texture_param_name = 'CoverTexture'

    # Load master material
    master_material = unreal.load_asset(master_material_path)
    if not master_material:
        unreal.log_error(f"ERROR: Master material not found at {master_material_path}")
        unreal.log_error("Please create M_VHSCover material first:")
        unreal.log_error("  1. Right-click in Content/CreatedMaterials -> Material")
        unreal.log_error("  2. Name it 'M_VHSCover'")
        unreal.log_error("  3. Add TextureSampleParameter2D node, name it 'CoverTexture'")
        unreal.log_error("  4. Connect RGB to Base Color, save")
        return

    unreal.log(f"Found master material: {master_material_path}")

    # Ensure output directory exists
    if not unreal.EditorAssetLibrary.does_directory_exist(materials_output_path):
        unreal.EditorAssetLibrary.make_directory(materials_output_path)

    # Get all assets in VHSCovers folder
    asset_paths = unreal.EditorAssetLibrary.list_assets(textures_path, recursive=False)

    created_count = 0
    skipped_count = 0

    unreal.log(f"Found {len(asset_paths)} assets in {textures_path}")

    for asset_path in asset_paths:
        # Skip the Materials subfolder
        if '/Materials/' in asset_path:
            continue

        # Convert ObjectPath to PackageName if needed (fixes deprecation warning)
        package_path = asset_path.split('.')[0] if '.' in asset_path else asset_path

        asset_data = unreal.EditorAssetLibrary.find_asset_data(package_path)
        if not asset_data:
            unreal.log(f"  Could not find asset data for: {package_path}")
            continue

        # Use asset_class_path (newer API) to avoid deprecation warning
        asset_class_path = asset_data.asset_class_path
        asset_class = str(asset_class_path.asset_name) if asset_class_path else ''

        unreal.log(f"  Asset: {package_path} -> Class: {asset_class}")

        # Only process Texture2D assets
        if 'Texture2D' not in asset_class:
            unreal.log(f"    Skipping - not a Texture2D")
            continue

        texture = unreal.load_asset(asset_path)
        if not texture:
            continue

        texture_name = texture.get_name()
        mi_name = f'MI_VHSCover_{texture_name}'
        mi_path = f'{materials_output_path}/{mi_name}'

        # Skip if material instance already exists
        if unreal.EditorAssetLibrary.does_asset_exist(mi_path):
            unreal.log(f"Skipping {mi_name} - already exists")
            skipped_count += 1
            continue

        # Create material instance
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

        mi = asset_tools.create_asset(
            mi_name,
            materials_output_path,
            unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew()
        )

        if mi:
            # Set the parent material
            mi.set_editor_property('parent', master_material)
            # Set the texture parameter
            unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
                mi,
                texture_param_name,
                texture
            )

            # Save the asset
            unreal.EditorAssetLibrary.save_asset(mi_path, only_if_is_dirty=False)
            unreal.log(f"Created {mi_name}")
            created_count += 1
        else:
            unreal.log_error(f"Failed to create material instance for {texture_name}")

    unreal.log(f"Done! Created {created_count} material instances, skipped {skipped_count} existing")

# Run the script
if __name__ == '__main__':
    create_vhs_material_instances()
