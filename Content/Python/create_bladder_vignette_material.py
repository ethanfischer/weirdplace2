"""
Creates M_BladderVignette — a post-process material that draws a yellow
vignette border around the screen edges (like a damage flash in an FPS).

C++ controls the "Intensity" scalar parameter (0 = no effect, 1 = full).
The blendable weight stays at 1.0 always.

Run in UE Editor Output Log:
  py "C:/Users/ethan/repos/weirdplace2/Content/Python/create_bladder_vignette_material.py"

Graph:
  Lerp(A=SceneColor, B=SceneColor + Yellow*Vignette, Alpha=Intensity) → Emissive
"""

import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

MATERIAL_PATH = '/Game/CreatedMaterials'
MATERIAL_NAME = 'M_BladderVignette'
FULL_PATH = f'{MATERIAL_PATH}/{MATERIAL_NAME}'


def connect(from_expr, to_expr, to_input, from_output=''):
    """Connect expressions and verify success."""
    result = mel.connect_material_expressions(from_expr, from_output, to_expr, to_input)
    if not result:
        unreal.log_warning(f'Failed to connect {from_expr.get_name()} -> {to_expr.get_name()}.{to_input}')
    return result


def create_bladder_vignette_material():
    if eal.does_asset_exist(FULL_PATH):
        unreal.log(f'{MATERIAL_NAME} already exists — deleting and recreating')
        eal.delete_asset(FULL_PATH)

    if not eal.does_directory_exist(MATERIAL_PATH):
        eal.make_directory(MATERIAL_PATH)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(
        MATERIAL_NAME, MATERIAL_PATH,
        unreal.Material, unreal.MaterialFactoryNew()
    )

    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)

    # --- Vignette mask computation ---

    # ScreenPosition (viewport UVs 0-1)
    screen_pos = mel.create_material_expression(material, unreal.MaterialExpressionScreenPosition, -1200, -200)

    # Center offset (0.5, 0.5)
    center = mel.create_material_expression(material, unreal.MaterialExpressionConstant2Vector, -1200, -100)
    center.set_editor_property('r', 0.5)
    center.set_editor_property('g', 0.5)

    # UV - center
    subtract = mel.create_material_expression(material, unreal.MaterialExpressionSubtract, -1000, -200)
    connect(screen_pos, subtract, 'A')
    connect(center, subtract, 'B')

    # Squared distance from center
    dot = mel.create_material_expression(material, unreal.MaterialExpressionDotProduct, -800, -200)
    connect(subtract, dot, 'A')
    connect(subtract, dot, 'B')

    # Scale by 3
    scale_const = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -800, -100)
    scale_const.set_editor_property('r', 3.0)
    scale_mul = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, -200)
    connect(dot, scale_mul, 'A')
    connect(scale_const, scale_mul, 'B')

    # Power 2 (sharpen falloff)
    exp_const = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -600, -100)
    exp_const.set_editor_property('r', 2.0)
    power = mel.create_material_expression(material, unreal.MaterialExpressionPower, -400, -200)
    connect(scale_mul, power, 'Base')
    connect(exp_const, power, 'Exponent')

    # Clamp 0-1
    clamp = mel.create_material_expression(material, unreal.MaterialExpressionClamp, -200, -200)
    clamp.set_editor_property('min_default', 0.0)
    clamp.set_editor_property('max_default', 1.0)
    connect(power, clamp, 'Input')

    # --- Yellow color ---

    yellow = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -200, -50)
    yellow.set_editor_property('constant', unreal.LinearColor(1.0, 0.7, 0.0, 1.0))

    # VignetteMask * Yellow
    color_mul = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, 0, -200)
    connect(clamp, color_mul, 'A')
    connect(yellow, color_mul, 'B')

    # --- Scene color ---

    scene_tex = mel.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -200, 200)
    scene_tex.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)

    # SceneColor + YellowVignette
    add = mel.create_material_expression(material, unreal.MaterialExpressionAdd, 200, 0)
    connect(scene_tex, add, 'A')
    connect(color_mul, add, 'B')

    # --- Intensity parameter (controls fade) ---

    intensity = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, 200, 200)
    intensity.set_editor_property('parameter_name', 'Intensity')
    intensity.set_editor_property('default_value', 0.0)

    # --- Lerp: original scene ↔ vignetted scene, controlled by Intensity ---

    lerp = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 400, 0)
    connect(scene_tex, lerp, 'A')      # Intensity=0 → original scene
    connect(add, lerp, 'B')            # Intensity=1 → vignetted scene
    connect(intensity, lerp, 'Alpha')  # Fade control

    # Output
    mel.connect_material_property(lerp, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    eal.save_asset(FULL_PATH, only_if_is_dirty=False)
    unreal.log(f'Created post-process material: {FULL_PATH}')


if __name__ == '__main__':
    create_bladder_vignette_material()
