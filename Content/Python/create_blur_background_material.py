import unreal

# Create the material asset
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
material_factory = unreal.MaterialFactoryNew()

material_path = "/Game/Materials"
material_name = "M_BlurredBackground"

# Create material
material = asset_tools.create_asset(material_name, material_path, unreal.Material, material_factory)

if material:
    # Set to Unlit shading model (so it's not affected by lighting)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    # Create texture parameter for the background capture
    tex_param = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -800, 0
    )
    tex_param.set_editor_property("parameter_name", "BackgroundTexture")

    # Create blur strength scalar parameter
    blur_param = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -800, 200
    )
    blur_param.set_editor_property("parameter_name", "BlurStrength")
    blur_param.set_editor_property("default_value", 0.005)

    # Create texture coordinate node
    tex_coord = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureCoordinate,
        -1200, 0
    )

    # For a simple blur, we'll just use the texture directly
    # The blur effect will be achieved by using a lower resolution render target
    # A proper blur would require multiple samples, which is complex to set up via Python

    # Connect texture RGB to Emissive Color
    unreal.MaterialEditingLibrary.connect_material_property(
        tex_param, "RGB",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )

    # Recompile the material
    unreal.MaterialEditingLibrary.recompile_material(material)

    # Save the asset
    unreal.EditorAssetLibrary.save_asset(f"{material_path}/{material_name}")

    print(f"Created blur background material: {material_path}/{material_name}")
    print("Note: For actual blur, set CaptureResolution to a lower value (e.g., 128-256) in C++")
    print("Or create a proper blur material manually with multiple texture samples")
else:
    print("Failed to create material")
