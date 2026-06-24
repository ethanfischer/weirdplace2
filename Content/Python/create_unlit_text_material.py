"""
Create M_UnlitText: an UNLIT version of the engine's default TextRender material,
so UTextRenderComponent labels (inventory item name, pause menu) stay readable in
dark areas without needing a fill light.

IMPORTANT: do NOT build this from scratch with a TextureSampleParameter2D — the
font glyph atlas is NOT bound to an arbitrary texture parameter by
UTextRenderComponent, so that renders a solid white block. Instead duplicate
/Engine/EngineMaterials/DefaultTextMaterialOpaque (which has the correct glyph /
distance-field opacity handling) and just:
  - switch shading model to Unlit
  - reroute its VertexColor (default: -> BaseColor) to EmissiveColor
VertexColor is the per-component SetTextRenderColor, so this gives white inventory
text AND the menu's focus-highlight colors, unlit (constant brightness).

Run headless:
    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=<abs path> -unattended -nopause -nosplash
"""

import unreal

SRC = "/Engine/EngineMaterials/DefaultTextMaterialOpaque"
DST = "/Game/Materials/M_UnlitText"

mel = unreal.MaterialEditingLibrary

if unreal.EditorAssetLibrary.does_asset_exist(DST):
    unreal.EditorAssetLibrary.delete_asset(DST)

material = unreal.EditorAssetLibrary.duplicate_asset(SRC, DST)
if not material:
    print("Failed to duplicate engine text material")
else:
    # Unlit so scene lighting no longer affects the text.
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    # The default material feeds VertexColor (= SetTextRenderColor) into BaseColor.
    # Unlit ignores BaseColor, so move that same node to EmissiveColor.
    color_node = mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    if color_node:
        mel.connect_material_property(color_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        # Fallback: drive emissive straight from vertex color.
        vc = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, 0)
        mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(DST)

    chk = unreal.load_object(None, DST)
    emissive = mel.get_material_property_input_node(chk, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    print("[unlit-text] shading=%s blend=%s emissive_connected=%s" % (
        chk.get_editor_property("shading_model"),
        chk.get_editor_property("blend_mode"),
        emissive is not None))
    print("Created unlit text material: " + DST)
