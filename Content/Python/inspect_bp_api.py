import unreal
print("=== BlueprintEditorLibrary methods ===")
for m in sorted(dir(unreal.BlueprintEditorLibrary)):
    if not m.startswith("_"):
        print(m)
print("=== Blueprint instance methods/props ===")
bp = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/BP_SettingsUIActor")
if bp:
    for m in sorted(dir(bp)):
        if "parent" in m.lower():
            print(m)
