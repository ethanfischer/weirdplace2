import unreal

MSB = unreal.get_engine_subsystem(unreal.MetaSoundBuilderSubsystem)
MSE = unreal.get_editor_subsystem(unreal.MetaSoundEditorSubsystem)
OK = unreal.MetaSoundBuilderResult.SUCCEEDED

def check(label, result):
    if result != OK:
        raise RuntimeError(f"FAILED: {label}: {result}")

builder, on_play, on_finished, audio_outs, result = MSB.create_source_builder(
    "MS_PassingMusicBuilder",
    output_format=unreal.MetaSoundOutputAudioFormat.STEREO,
    is_one_shot=False,
)
check("create_source_builder", result)
print(f"audio_outs: {len(audio_outs)}")

def add_node(namespace, name, variant=""):
    cn = unreal.MetasoundFrontendClassName(namespace=namespace, name=name, variant=variant)
    node, r = builder.add_node_by_class_name(cn, 1)
    check(f"add_node {namespace}.{name}.{variant}", r)
    return node

def nin(node, name):
    h, r = builder.find_node_input_by_name(node, name)
    check(f"find input '{name}'", r)
    return h

def nout(node, name):
    h, r = builder.find_node_output_by_name(node, name)
    check(f"find output '{name}'", r)
    return h

intro_node = add_node("UE", "Wave Player", "Stereo")
loop_node = add_node("UE", "Wave Player", "Stereo")

intro_wave = unreal.load_asset("/Game/Sounds/Passing_Intro")
loop_wave = unreal.load_asset("/Game/Sounds/Passing_Loop")
assert intro_wave and loop_wave, "wave assets missing"

check("set intro wave", builder.set_node_input_default(nin(intro_node, "Wave Asset"), MSB.create_object_meta_sound_literal(intro_wave)))
check("set loop wave", builder.set_node_input_default(nin(loop_node, "Wave Asset"), MSB.create_object_meta_sound_literal(loop_wave)))
check("set loop flag", builder.set_node_input_default(nin(loop_node, "Loop"), MSB.create_bool_meta_sound_literal(True)[0]))

check("OnPlay->intro.Play", builder.connect_nodes(on_play, nin(intro_node, "Play")))
check("intro.OnFinished->loop.Play", builder.connect_nodes(nout(intro_node, "On Finished"), nin(loop_node, "Play")))

add_l = add_node("UE", "Add", "Audio")
add_r = add_node("UE", "Add", "Audio")

check("introL->addL", builder.connect_nodes(nout(intro_node, "Out Left"), nin(add_l, "PrimaryOperand")))
check("loopL->addL", builder.connect_nodes(nout(loop_node, "Out Left"), nin(add_l, "AdditionalOperands")))
check("introR->addR", builder.connect_nodes(nout(intro_node, "Out Right"), nin(add_r, "PrimaryOperand")))
check("loopR->addR", builder.connect_nodes(nout(loop_node, "Out Right"), nin(add_r, "AdditionalOperands")))

check("addL->graph L", builder.connect_nodes(nout(add_l, "Out"), audio_outs[0]))
check("addR->graph R", builder.connect_nodes(nout(add_r, "Out"), audio_outs[1]))

doc, result = MSE.build_to_asset(builder, "Ethan Fischer", "MS_PassingMusic", "/Game/Sounds")
check("build_to_asset", result)
unreal.EditorAssetLibrary.save_asset("/Game/Sounds/MS_PassingMusic")
print("DONE: /Game/Sounds/MS_PassingMusic")
