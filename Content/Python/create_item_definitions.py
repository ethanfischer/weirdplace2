"""
One-shot migration: create UItemDefinition data assets for the four held items
(Money, Key, BathroomKey, BrokenKey) and wire them onto the level instances of
Rick / Seneca / OutsideBathroomDoor / BathroomKey.

The old MoneyMesh/KeyMesh/etc. UPROPERTYs on those actors were removed in C++,
so any custom Scale/NotificationRotation overrides on the level instances are
gone — verify and retune in the editor afterward.

Run headless:
    UnrealEditor-Cmd.exe weirdplace2.uproject \
        -ExecutePythonScript=Content/Python/create_item_definitions.py \
        -unattended -nopause -nosplash
"""

import unreal


# -----------------------------------------------------------------------------
# Asset paths
# -----------------------------------------------------------------------------
DEST_DIR    = "/Game/Inventory"
LEVEL_PATH  = "/Game/FirstPerson/Maps/FirstPersonMap"

CASH_MESH      = "/Game/Import/cash/cash"
KEY_MESH       = "/Game/Fab/Small_Key__1MB_/small_key_1mb"
BROKEN_KEY     = "/Game/Fab/Small_Key__1MB_/brokenkey"
KEY_MATERIAL   = "/Game/Fab/Small_Key__1MB_/MetalKey"

# Each entry: asset_name -> (item_id, mesh_path, materials, scale)
DEFS = {
    "DA_Money":       ("Money",       CASH_MESH,  [],              unreal.Vector(1.0, 1.0, 1.0)),
    "DA_Key":         ("Key",         KEY_MESH,   [KEY_MATERIAL],  unreal.Vector(0.001, 0.001, 0.001)),
    "DA_BathroomKey": ("BathroomKey", KEY_MESH,   [KEY_MATERIAL],  unreal.Vector(0.001, 0.001, 0.001)),
    "DA_BrokenKey":   ("BrokenKey",   BROKEN_KEY, [KEY_MATERIAL],  unreal.Vector(0.001, 0.001, 0.001)),
}


def load_or_die(path, expected_class=None):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        unreal.log_error(f"[migrate] missing asset: {path}")
        return None
    if expected_class and not isinstance(asset, expected_class):
        unreal.log_error(f"[migrate] {path} is {type(asset).__name__}, expected {expected_class.__name__}")
        return None
    return asset


def ensure_definition(asset_name, item_id, mesh_path, material_paths, scale):
    """Create-or-update one DA_* asset with the given fields."""
    full_path = f"{DEST_DIR}/{asset_name}"

    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        asset = unreal.EditorAssetLibrary.load_asset(full_path)
        unreal.log(f"[migrate] reusing existing {full_path}")
    else:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        asset = tools.create_asset(asset_name, DEST_DIR, unreal.ItemDefinition, None)
        if not asset:
            unreal.log_error(f"[migrate] failed to create {full_path}")
            return None
        unreal.log(f"[migrate] created {full_path}")

    mesh = load_or_die(mesh_path, unreal.StaticMesh)
    if not mesh:
        return None

    mat_overrides = []
    for mp in material_paths:
        mat = load_or_die(mp)
        if mat:
            mat_overrides.append(mat)

    asset.set_editor_property("item_id", unreal.Name(item_id))
    asset.set_editor_property("mesh", mesh)
    asset.set_editor_property("material_overrides", mat_overrides)
    asset.set_editor_property("scale", scale)
    asset.set_editor_property("held_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    asset.set_editor_property("notification_rotation", unreal.Rotator(0.0, 0.0, 0.0))

    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return asset


def assign_on_level_actors(defs):
    """For every loaded level actor, set the matching *Def property if present."""
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = eas.get_all_level_actors()
    unreal.log(f"[migrate] loaded {len(actors)} actor(s) from {LEVEL_PATH}")

    # (python_class, property_name, def_asset). isinstance handles BP subclasses.
    bindings = [
        (unreal.Rick,                "money_def",      defs["DA_Money"]),
        (unreal.Seneca,              "key_def",        defs["DA_Key"]),
        (unreal.BathroomKey,         "item_def",       defs["DA_BathroomKey"]),
        (unreal.OutsideBathroomDoor, "broken_key_def", defs["DA_BrokenKey"]),
    ]

    touched_packages = set()
    matched_any = False
    for actor in actors:
        for target_class, prop, def_asset in bindings:
            if def_asset is None:
                continue
            if not isinstance(actor, target_class):
                continue
            try:
                actor.set_editor_property(prop, def_asset)
                unreal.log(f"[migrate] {actor.get_class().get_name()} '{actor.get_actor_label()}'.{prop} = {def_asset.get_name()}")
                touched_packages.add(actor.get_outer().get_package())
                matched_any = True
            except Exception as e:
                unreal.log_error(f"[migrate] failed to set {actor.get_class().get_name()}.{prop}: {e}")

    if not matched_any:
        unique = sorted({a.get_class().get_name() for a in actors})
        unreal.log_warning(f"[migrate] no actors matched. Unique classes in level: {unique}")

    for pkg in touched_packages:
        unreal.EditorLoadingAndSavingUtils.save_packages([pkg], False)
    unreal.log(f"[migrate] saved {len(touched_packages)} package(s)")


def main():
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST_DIR):
        unreal.EditorAssetLibrary.make_directory(DEST_DIR)

    defs = {}
    for name, (item_id, mesh_path, mats, scale) in DEFS.items():
        defs[name] = ensure_definition(name, item_id, mesh_path, mats, scale)

    if any(d is None for d in defs.values()):
        unreal.log_error("[migrate] aborting level wiring — some defs failed to create")
        return

    assign_on_level_actors(defs)
    unreal.log("[migrate] done")


if __name__ == "__main__":
    main()
