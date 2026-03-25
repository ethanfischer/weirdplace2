"""
Create a simple unlit material for displaying item thumbnails in the inventory UI.

Unlike M_VHSCoverFront, this shows the texture at full UV (no cropping or rotation).
Use this for non-VHS items like keys, quest objects, etc.

Run in UE Editor Output Log:
    py "C:/Users/ethan/repos/weirdplace2/Content/Python/create_item_thumbnail_material.py"

Thumbnail convention: place textures at /Game/Images/ItemThumbnails/<ItemID>_thumbnail
The inventory will automatically pick them up (e.g. ItemID "Key" -> key_thumbnail).
"""

import unreal


def create_item_thumbnail_material():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary

    material_path = "/Game/Materials"
    material_name = "M_ItemThumbnail"
    full_path = f"{material_path}/{material_name}"

    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        print(f"Material {full_path} already exists - deleting and recreating...")
        unreal.EditorAssetLibrary.delete_asset(full_path)

    material_factory = unreal.MaterialFactoryNew()
    material = asset_tools.create_asset(material_name, material_path, unreal.Material, material_factory)

    if not material:
        print("Failed to create material!")
        return

    # Unlit with exposure multiplier: black in the texture stays pure black (0 × anything = 0).
    # Lit shading would brighten the background via the inventory flashlight.

    # The plane mesh when rotated 90° pitch maps UVs rotated 90°.
    # Apply the same (u,v) -> (v, 1-u) correction used by M_VHSCoverFront.
    tex_coord = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -700, 0)

    mask_u = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -600, -50)
    mask_u.set_editor_property("r", True)
    mask_u.set_editor_property("g", False)

    mask_v = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -600, 50)
    mask_v.set_editor_property("r", False)
    mask_v.set_editor_property("g", True)

    one_minus_u = mel.create_material_expression(material, unreal.MaterialExpressionOneMinus, -500, -50)

    rotated_uv = mel.create_material_expression(material, unreal.MaterialExpressionAppendVector, -400, 0)

    mel.connect_material_expressions(tex_coord, "", mask_u, "")
    mel.connect_material_expressions(tex_coord, "", mask_v, "")
    mel.connect_material_expressions(mask_u, "", one_minus_u, "")
    mel.connect_material_expressions(mask_v, "", rotated_uv, "A")
    mel.connect_material_expressions(one_minus_u, "", rotated_uv, "B")

    # Texture sample with corrected UVs
    texture_param = mel.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -200, 0
    )
    texture_param.set_editor_property("parameter_name", "ThumbnailTexture")
    mel.connect_material_expressions(rotated_uv, "", texture_param, "UVs")

    # Exposure multiplier — black stays black, key brightness is tunable
    exposure = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -50, 80)
    exposure.set_editor_property("parameter_name", "Exposure")
    exposure.set_editor_property("default_value", 0.25)

    multiply = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, 100, 0)
    mel.connect_material_expressions(texture_param, "RGB", multiply, "A")
    mel.connect_material_expressions(exposure, "", multiply, "B")

    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(full_path)

    print(f"Created: {full_path}")
    print("Thumbnail convention: /Game/Images/ItemThumbnails/<ItemID>_thumbnail")


if __name__ == "__main__":
    create_item_thumbnail_material()
