import unreal
result = unreal.EditorLevelLibrary.save_current_level()
with open(r'C:\Users\ethan\repos\weirdplace2\Saved\inspect_chord_out.txt', 'w') as f:
    f.write('save_current_level: ' + str(result))
