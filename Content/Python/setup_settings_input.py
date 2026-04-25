"""
Wires the IA_Settings input action into IMC_Default and BP_FirstPersonCharacter.

Idempotent: skips work that's already done. Safe to re-run.

Run in UE Editor:
    py "Content/Python/setup_settings_input.py"

What it does:
1. Ensures /Game/FirstPerson/Input/Actions/IA_Settings exists (UInputAction, Boolean).
2. Ensures /Game/FirstPerson/Input/IMC_Default has IA_Settings mapped to:
     - Gamepad Special Right (Start button)
     - Escape
3. Sets BP_FirstPersonCharacter.SettingsAction default to IA_Settings.
"""

import unreal


IA_SETTINGS_PATH = "/Game/FirstPerson/Input/Actions/IA_Settings"
IMC_DEFAULT_PATH = "/Game/FirstPerson/Input/IMC_Default"
BP_FIRST_PERSON_PATH = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"


def ensure_ia_settings():
    """Step 1: Make sure IA_Settings exists as a Boolean InputAction."""
    if unreal.EditorAssetLibrary.does_asset_exist(IA_SETTINGS_PATH):
        ia = unreal.EditorAssetLibrary.load_asset(IA_SETTINGS_PATH)
        unreal.log(f"IA_Settings already exists at {IA_SETTINGS_PATH}")
        return ia

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    ia = asset_tools.create_asset(
        asset_name="IA_Settings",
        package_path="/Game/FirstPerson/Input/Actions",
        asset_class=unreal.InputAction,
        factory=unreal.InputActionFactory(),
    )
    if not ia:
        raise RuntimeError("Failed to create IA_Settings asset")

    ia.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    unreal.EditorAssetLibrary.save_asset(IA_SETTINGS_PATH, only_if_is_dirty=False)
    unreal.log(f"Created IA_Settings at {IA_SETTINGS_PATH}")
    return ia


def make_key(name):
    """Construct an FKey by name (UE 5.4 Python: Key takes 0 positional args)."""
    k = unreal.Key()
    k.set_editor_property("key_name", name)
    return k


def ensure_imc_binding(ia_settings):
    """Step 2: Make sure IMC_Default maps IA_Settings to Start + Escape."""
    imc = unreal.EditorAssetLibrary.load_asset(IMC_DEFAULT_PATH)
    if not imc:
        raise RuntimeError(f"Could not load {IMC_DEFAULT_PATH}")

    desired_key_names = ["Gamepad_Special_Right", "Escape"]

    mappings = list(imc.get_editor_property("mappings") or [])
    existing_keys = set()
    for m in mappings:
        action = m.get_editor_property("action")
        if action == ia_settings:
            key = m.get_editor_property("key")
            existing_keys.add(str(key.get_editor_property("key_name")))

    added = 0
    for key_name in desired_key_names:
        if key_name in existing_keys:
            unreal.log(f"IMC mapping already present: IA_Settings -> {key_name}")
            continue
        new_mapping = unreal.EnhancedActionKeyMapping()
        new_mapping.set_editor_property("action", ia_settings)
        new_mapping.set_editor_property("key", make_key(key_name))
        mappings.append(new_mapping)
        added += 1
        unreal.log(f"Added IMC mapping: IA_Settings -> {key_name}")

    if added > 0:
        imc.set_editor_property("mappings", mappings)
        unreal.EditorAssetLibrary.save_asset(IMC_DEFAULT_PATH, only_if_is_dirty=False)
        unreal.log(f"IMC_Default updated with {added} new IA_Settings mapping(s)")
    else:
        unreal.log("IMC_Default already has all IA_Settings mappings")


def ensure_bp_settings_action(ia_settings):
    """Step 3: Set BP_FirstPersonCharacter.SettingsAction default to IA_Settings."""
    bp = unreal.EditorAssetLibrary.load_asset(BP_FIRST_PERSON_PATH)
    if not bp:
        raise RuntimeError(f"Could not load {BP_FIRST_PERSON_PATH}")

    cdo = unreal.get_default_object(bp.generated_class())
    current = cdo.get_editor_property("SettingsAction")
    if current == ia_settings:
        unreal.log("BP_FirstPersonCharacter.SettingsAction already set to IA_Settings")
        return

    cdo.set_editor_property("SettingsAction", ia_settings)
    # Mark blueprint dirty + compile + save
    unreal.EditorAssetLibrary.save_asset(BP_FIRST_PERSON_PATH, only_if_is_dirty=False)
    bp_compile = unreal.BlueprintEditorLibrary
    if hasattr(bp_compile, "compile_blueprint"):
        bp_compile.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_asset(BP_FIRST_PERSON_PATH, only_if_is_dirty=False)
    unreal.log("BP_FirstPersonCharacter.SettingsAction <- IA_Settings")


def main():
    unreal.log("=== setup_settings_input.py ===")
    ia = ensure_ia_settings()
    ensure_imc_binding(ia)
    ensure_bp_settings_action(ia)
    unreal.log("=== Done ===")


main()
