"""Force-recompile BPs that may be stale after C++ schema changes."""
import unreal


PATHS = [
    "/Game/Blueprints/BP_MenuUIActor",
    "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter",
]


def main():
    for path in PATHS:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log_warning(f"Missing: {path}")
            continue
        bp = unreal.EditorAssetLibrary.load_asset(path)
        if not bp:
            unreal.log_warning(f"Failed to load: {path}")
            continue
        unreal.log(f"Compiling {path}")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
    unreal.log("Recompile done")


main()
