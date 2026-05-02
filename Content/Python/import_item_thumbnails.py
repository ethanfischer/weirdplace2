"""
Import chosen rendered PNGs as the inventory thumbnail Texture2D assets that
AInventoryUIActor::CreateThumbnails looks up by convention path.

Convention: /Game/Images/ItemThumbnails/<ItemID>_thumbnail

Edit the IMPORTS list below to remap which generated PNG becomes which item's
thumbnail, then re-run.

Run via:
    py "C:/Users/ethan/repos/weirdplace2/Content/Python/import_item_thumbnails.py"
"""

import os
import unreal


# (source PNG filename, destination asset name) — destination dir is fixed below.
IMPORTS = [
    # Cash: head-on bill face from the rendered set.
    ("Money_p0_y0_r45.png", "Money_thumbnail"),
    # Key: 3/4 metallic angle. Matches the existing lowercase asset name on
    # disk so we overwrite cleanly instead of creating a second file.
    ("Key_p45_y90_r90.png", "key_thumbnail"),
]

SCREENSHOT_DIR = os.path.join(unreal.SystemLibrary.get_project_saved_directory(), "Screenshots")
DEST_PATH = "/Game/Images/ItemThumbnails"


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for filename, dest_name in IMPORTS:
        src = os.path.join(SCREENSHOT_DIR, filename)
        if not os.path.isfile(src):
            unreal.log_error(f"[import] missing source: {src}")
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", src)
        task.set_editor_property("destination_path", DEST_PATH)
        task.set_editor_property("destination_name", dest_name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("factory", unreal.TextureFactory())
        tasks.append(task)
        unreal.log(f"[import] queued {filename} -> {DEST_PATH}/{dest_name}")

    if not tasks:
        return

    asset_tools.import_asset_tasks(tasks)
    unreal.log(f"[import] Done: {len(tasks)} thumbnails imported. Save package via editor.")


if __name__ == "__main__":
    main()
