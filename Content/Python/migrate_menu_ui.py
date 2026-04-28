"""
Migrates BP_SettingsUIActor -> BP_MenuUIActor (reparented to AMenuUIActor),
and points BP_FirstPersonCharacter.MenuUIComponent.MenuUIActorClass at it.

Idempotent: skips work that's already done. Safe to re-run.
"""

import unreal


OLD_BP_PATH = "/Game/Blueprints/BP_SettingsUIActor"
NEW_BP_PATH = "/Game/Blueprints/BP_MenuUIActor"
BP_FIRST_PERSON_PATH = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"


def reparent_and_rename():
    asset_lib = unreal.EditorAssetLibrary

    new_exists = asset_lib.does_asset_exist(NEW_BP_PATH)
    old_exists = asset_lib.does_asset_exist(OLD_BP_PATH)

    target_path = NEW_BP_PATH if new_exists else OLD_BP_PATH
    if not new_exists and not old_exists:
        unreal.log_error(f"Neither {NEW_BP_PATH} nor {OLD_BP_PATH} exists")
        return None

    bp = asset_lib.load_asset(target_path)
    if not bp:
        unreal.log_error(f"Failed to load {target_path}")
        return None

    new_parent = unreal.load_class(None, "/Script/weirdplace2.MenuUIActor")
    if not new_parent:
        unreal.log_error("Could not load AMenuUIActor class — is it built?")
        return None

    unreal.log(f"Reparenting {target_path} -> AMenuUIActor (idempotent)")
    unreal.BlueprintEditorLibrary.reparent_blueprint(bp, new_parent)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)

    if target_path == OLD_BP_PATH:
        unreal.log(f"Renaming {OLD_BP_PATH} -> {NEW_BP_PATH}")
        if not asset_lib.rename_asset(OLD_BP_PATH, NEW_BP_PATH):
            unreal.log_error("Rename failed")
            return None

    return asset_lib.load_asset(NEW_BP_PATH)


def wire_first_person_default(menu_bp):
    asset_lib = unreal.EditorAssetLibrary
    if not asset_lib.does_asset_exist(BP_FIRST_PERSON_PATH):
        unreal.log_error(f"{BP_FIRST_PERSON_PATH} not found")
        return

    bp = asset_lib.load_asset(BP_FIRST_PERSON_PATH)
    if not bp:
        unreal.log_error(f"Failed to load {BP_FIRST_PERSON_PATH}")
        return

    cdo = unreal.get_default_object(bp.generated_class())
    menu_comp = cdo.get_editor_property("MenuUIComponent")
    if not menu_comp:
        unreal.log_error("BP_FirstPersonCharacter has no MenuUIComponent — did the build run?")
        return

    new_class = menu_bp.generated_class()
    current = menu_comp.get_editor_property("MenuUIActorClass")
    if current == new_class:
        unreal.log("MenuUIActorClass already set to BP_MenuUIActor")
        return

    menu_comp.set_editor_property("MenuUIActorClass", new_class)
    asset_lib.save_loaded_asset(bp)
    unreal.log("Set BP_FirstPersonCharacter.MenuUIComponent.MenuUIActorClass <- BP_MenuUIActor")


def main():
    unreal.log("=== migrate_menu_ui starting ===")
    menu_bp = reparent_and_rename()
    if menu_bp:
        wire_first_person_default(menu_bp)
    unreal.log("=== migrate_menu_ui done ===")


main()
