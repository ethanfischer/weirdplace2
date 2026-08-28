import unreal

tasks = []
for name in ("Passing_Intro", "Passing_Loop"):
    t = unreal.AssetImportTask()
    t.filename = f"C:/Users/ethan/repos/weirdplace2/Content/Sounds/{name}.wav"
    t.destination_path = "/Game/Sounds"
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for name in ("Passing_Intro", "Passing_Loop"):
    path = f"/Game/Sounds/{name}"
    wave = unreal.load_asset(path)
    if not wave:
        print(f"ERROR: import failed for {path}")
    else:
        print(f"OK: {path} duration={wave.get_editor_property('duration'):.2f}s")

# Loop asset must loop at the wave level too (belt for non-builder playback paths)
loop = unreal.load_asset("/Game/Sounds/Passing_Loop")
if loop:
    loop.set_editor_property("looping", True)
    unreal.EditorAssetLibrary.save_asset("/Game/Sounds/Passing_Loop")
    print("OK: Passing_Loop looping=True saved")
