import unreal

# Create the material asset
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
material_factory = unreal.MaterialFactoryNew()

material_path = "/Game/Materials"
material_name = "M_UnlitText"

# Create material
material = asset_tools.create_asset(material_name, material_path, unreal.Material, material_factory)

if material:
    # Set to Unlit shading model and Masked blend mode
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)

    # Create texture parameter node for FontTexture
    tex_param = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -400, 0
    )
    tex_param.set_editor_property("parameter_name", "FontTexture")

    # Connect RGB to Emissive Color
    unreal.MaterialEditingLibrary.connect_material_property(
        tex_param, "RGB",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    # Connect Alpha to Opacity Mask
    unreal.MaterialEditingLibrary.connect_material_property(
        tex_param, "A",
        unreal.MaterialProperty.MP_OPACITY_MASK
    )

    # Recompile the material
    unreal.MaterialEditingLibrary.recompile_material(material)

    # Save the asset
    unreal.EditorAssetLibrary.save_asset(f"{material_path}/{material_name}")

    print(f"Created unlit text material: {material_path}/{material_name}")
else:
    print("Failed to create material")
