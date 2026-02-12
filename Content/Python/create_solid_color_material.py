"""
Create a simple solid color material with an exposed Color parameter.
Run in UE Editor: py "Content/Python/create_solid_color_material.py"
"""

import unreal

def create_solid_color_material():
    # Get the asset tools
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    # Create material factory
    material_factory = unreal.MaterialFactoryNew()

    # Create the material asset
    material_path = "/Game/Materials"
    material_name = "M_SolidColor"

    # Check if it already exists
    if unreal.EditorAssetLibrary.does_asset_exist(f"{material_path}/{material_name}"):
        print(f"Material {material_path}/{material_name} already exists!")
        return

    # Create the material
    material = asset_tools.create_asset(material_name, material_path, unreal.Material, material_factory)

    if material:
        # We need to use material editing subsystem
        mel = unreal.MaterialEditingLibrary

        # Create a VectorParameter node for color
        color_param = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -300, 0)
        color_param.set_editor_property("parameter_name", "Color")
        color_param.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

        # Connect to Base Color
        mel.connect_material_property(color_param, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

        # Also connect to Emissive for brightness
        emissive_param = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -300, 200)
        emissive_param.set_editor_property("parameter_name", "EmissiveColor")
        emissive_param.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
        mel.connect_material_property(emissive_param, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

        # Set material to unlit for solid color appearance
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

        # Recompile the material
        mel.recompile_material(material)

        # Save the asset
        unreal.EditorAssetLibrary.save_asset(f"{material_path}/{material_name}")

        print(f"Created material: {material_path}/{material_name}")
        print("You can now use '/Game/Materials/M_SolidColor' in your code")
    else:
        print("Failed to create material!")

if __name__ == "__main__":
    create_solid_color_material()
