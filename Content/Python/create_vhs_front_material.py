"""
Create a material for displaying only the FRONT FACE of VHS covers in the inventory UI.

This material uses UV offset/scale to sample only the front portion of VHS cover textures
which are laid out for wrapping around a 3D box.

Run in UE Editor: py "C:/Users/ethan/repos/weirdplace2/Content/Python/create_vhs_front_material.py"
"""

import unreal


def create_vhs_front_material():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary

    material_path = "/Game/Materials"
    material_name = "M_VHSCoverFront"
    full_path = f"{material_path}/{material_name}"

    # Check if it already exists
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        print(f"Material {full_path} already exists - deleting and recreating...")
        unreal.EditorAssetLibrary.delete_asset(full_path)

    # Create the material
    material_factory = unreal.MaterialFactoryNew()
    material = asset_tools.create_asset(material_name, material_path, unreal.Material, material_factory)

    if not material:
        print("Failed to create material!")
        return

    # Create TextureSampleParameter2D for the cover texture
    texture_param = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0)
    texture_param.set_editor_property("parameter_name", "CoverTexture")

    # Create TexCoord node for UV manipulation
    tex_coord = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1400, 0)

    # Rotate UVs 90 degrees: (U, V) -> (V, 1-U)
    # Break out U and V components
    component_mask_u = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1300, -50)
    component_mask_u.set_editor_property("r", True)
    component_mask_u.set_editor_property("g", False)

    component_mask_v = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1300, 50)
    component_mask_v.set_editor_property("r", False)
    component_mask_v.set_editor_property("g", True)

    # 1 - U for the new V
    one_minus_u = mel.create_material_expression(material, unreal.MaterialExpressionOneMinus, -1200, -50)

    # Append to make rotated UV: (V, 1-U)
    rotated_uv = mel.create_material_expression(material, unreal.MaterialExpressionAppendVector, -1100, 0)

    mel.connect_material_expressions(tex_coord, "", component_mask_u, "")
    mel.connect_material_expressions(tex_coord, "", component_mask_v, "")
    mel.connect_material_expressions(component_mask_u, "", one_minus_u, "")
    mel.connect_material_expressions(component_mask_v, "", rotated_uv, "A")
    mel.connect_material_expressions(one_minus_u, "", rotated_uv, "B")

    # Create Multiply node for UV scale
    multiply = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -900, 0)

    # Create Add node for UV offset
    add = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -700, 0)

    # Create ScalarParameter for U Scale (front face is 45% of texture width)
    u_scale_param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1100, 100)
    u_scale_param.set_editor_property("parameter_name", "UScale")
    u_scale_param.set_editor_property("default_value", 0.45)

    # Create ScalarParameter for V Scale (full height)
    v_scale_param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1100, 150)
    v_scale_param.set_editor_property("parameter_name", "VScale")
    v_scale_param.set_editor_property("default_value", 1.0)

    # Create ScalarParameter for U Offset (front face starts at 55%)
    u_offset_param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 200)
    u_offset_param.set_editor_property("parameter_name", "UOffset")
    u_offset_param.set_editor_property("default_value", 0.55)

    # Create ScalarParameter for V Offset (no vertical offset needed)
    v_offset_param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 250)
    v_offset_param.set_editor_property("parameter_name", "VOffset")
    v_offset_param.set_editor_property("default_value", 0.0)

    # Create Append node to combine U and V scale into a 2D vector
    append_scale = mel.create_material_expression(material, unreal.MaterialExpressionAppendVector, -1000, 125)

    # Create Append node to combine U and V offset into a 2D vector
    append_offset = mel.create_material_expression(material, unreal.MaterialExpressionAppendVector, -800, 225)

    # --- Aspect-preserving pillarbox -------------------------------------
    # The slot plane is square (1:1) but the sampled front face is portrait
    # (CoverAspect = front width/height < 1), so mapping it full-UV stretches it
    # horizontally. Shrink the cover's WIDTH axis (rotated_uv.x, = V) into the
    # centered CoverAspect fraction and black out the rest -> black bars L/R,
    # cover fills the slot vertically at its true aspect.
    #
    # CoverAspect = (UScale * texW) / (VScale * texH) = (0.45 * 470) / (1.0 * 367)
    # for the uniform 470x367 cover textures. Recompute if UScale / texture dims change.
    cover_aspect = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1000, 320)
    cover_aspect.set_editor_property("parameter_name", "CoverAspect")
    cover_aspect.set_editor_property("default_value", 0.5763)

    # centerX = V - 0.5
    half_const = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -1100, 280)
    half_const.set_editor_property("r", 0.5)
    center_x = mel.create_material_expression(material, unreal.MaterialExpressionSubtract, -950, 0)
    mel.connect_material_expressions(component_mask_v, "", center_x, "A")
    mel.connect_material_expressions(half_const, "", center_x, "B")

    # remappedX = centerX / CoverAspect + 0.5
    remap_div = mel.create_material_expression(material, unreal.MaterialExpressionDivide, -850, 0)
    mel.connect_material_expressions(center_x, "", remap_div, "A")
    mel.connect_material_expressions(cover_aspect, "", remap_div, "B")
    remap_add = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -750, 0)
    mel.connect_material_expressions(remap_div, "", remap_add, "A")
    mel.connect_material_expressions(half_const, "", remap_add, "B")

    # Rebuild the UV with the pillarboxed width axis: (remappedX, 1-U)
    pillarboxed_uv = mel.create_material_expression(material, unreal.MaterialExpressionAppendVector, -650, 0)
    mel.connect_material_expressions(remap_add, "", pillarboxed_uv, "A")
    mel.connect_material_expressions(one_minus_u, "", pillarboxed_uv, "B")

    # mask = 1 inside the centered cover region (|centerX| <= CoverAspect/2), else 0.
    # Implemented as clamp((half - |centerX|) * BIG) so it's ~1 inside and snaps to
    # 0 just past the edge (ramp width ~1/BIG is sub-pixel) -> crisp black bars.
    abs_center = mel.create_material_expression(material, unreal.MaterialExpressionAbs, -850, 320)
    mel.connect_material_expressions(center_x, "", abs_center, "")
    half_aspect = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -850, 400)
    mel.connect_material_expressions(cover_aspect, "", half_aspect, "A")
    mel.connect_material_expressions(half_const, "", half_aspect, "B")
    edge_dist = mel.create_material_expression(material, unreal.MaterialExpressionSubtract, -700, 360)
    mel.connect_material_expressions(half_aspect, "", edge_dist, "A")
    mel.connect_material_expressions(abs_center, "", edge_dist, "B")
    big_const = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -700, 440)
    big_const.set_editor_property("r", 100000.0)
    edge_scaled = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -580, 380)
    mel.connect_material_expressions(edge_dist, "", edge_scaled, "A")
    mel.connect_material_expressions(big_const, "", edge_scaled, "B")
    bar_mask = mel.create_material_expression(material, unreal.MaterialExpressionClamp, -460, 380)
    mel.connect_material_expressions(edge_scaled, "", bar_mask, "")

    # Connect the nodes
    # Scale: PillarboxedUV * (UScale, VScale)
    mel.connect_material_expressions(u_scale_param, "", append_scale, "A")
    mel.connect_material_expressions(v_scale_param, "", append_scale, "B")
    mel.connect_material_expressions(pillarboxed_uv, "", multiply, "A")
    mel.connect_material_expressions(append_scale, "", multiply, "B")

    # Offset: (scaled UV) + (UOffset, VOffset)
    mel.connect_material_expressions(u_offset_param, "", append_offset, "A")
    mel.connect_material_expressions(v_offset_param, "", append_offset, "B")
    mel.connect_material_expressions(multiply, "", add, "A")
    mel.connect_material_expressions(append_offset, "", add, "B")

    # Connect to texture sampler UVs
    mel.connect_material_expressions(add, "", texture_param, "UVs")

    # Self-illuminated (unlit) so VHS-cover thumbnails don't depend on the
    # inventory RectLight. Mirrors M_ItemThumbnail: texture * Exposure, then
    # divide by EyeAdaptation to cancel tonemapper auto-exposure so brightness
    # stays constant regardless of how dim/bright the surrounding scene is.
    exposure = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -300, 200)
    exposure.set_editor_property("parameter_name", "Exposure")
    exposure.set_editor_property("default_value", 1.0)

    multiply_exposure = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -150, 0)
    mel.connect_material_expressions(texture_param, "RGB", multiply_exposure, "A")
    mel.connect_material_expressions(exposure, "", multiply_exposure, "B")

    eye_adapt = mel.create_material_expression(material, unreal.MaterialExpressionEyeAdaptation, -150, 200)
    divide = mel.create_material_expression(material, unreal.MaterialExpressionDivide, 50, 0)
    mel.connect_material_expressions(multiply_exposure, "", divide, "A")
    mel.connect_material_expressions(eye_adapt, "", divide, "B")

    # Apply the pillarbox mask: black (0) outside the centered cover region.
    masked = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, 250, 0)
    mel.connect_material_expressions(divide, "", masked, "A")
    mel.connect_material_expressions(bar_mask, "", masked, "B")

    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mel.connect_material_property(masked, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Recompile and save
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(full_path)

    print(f"Created material: {full_path}")
    print("")
    print("UV settings for VHS front face:")
    print(f"  UScale: 0.45 (front face is 45% of texture width)")
    print(f"  VScale: 1.0 (full height)")
    print(f"  UOffset: 0.55 (front face starts at 55%)")
    print(f"  VOffset: 0.0 (no vertical offset)")
    print("")
    print("You can adjust these via Material Instance parameters if needed.")


if __name__ == "__main__":
    create_vhs_front_material()
