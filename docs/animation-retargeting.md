# Retargeting mocap animations onto MetaHumans

How to take a UE5-Mannequin-skeleton animation (e.g. anything from the mocapcentral
free pack) and play it on a MetaHuman, end to end, without clicking through the
IK Retargeter editor UI.

The orchestration script is `scripts/local/retarget_mocap_to_metahuman.py`. It is
idempotent — re-running cleans up its own intermediate artifacts.

## TL;DR for a new animation

1. Drop the anim's `.uasset` into `Content/Animations/` (or wherever).
2. Confirm what skeleton it references (see "Identifying the source skeleton" below).
3. If that skeleton isn't already in the project, copy the skeleton `.uasset` (and
   ideally its companion SkeletalMesh `.uasset`) from the source pack into the
   exact `/Game/...` path the anim expects.
4. Edit the constants at the top of `retarget_mocap_to_metahuman.py`:
   - `SRC_ANIM_PATH` — the new anim
   - (only if it's a new pack) `MCUE5_SKELETON` / `MCUE5_MESH` / the source IK Rig
   - `TGT_MESH` — the MetaHuman body mesh whose AnimBP we're swapping into
   - `ABP_PATH` — the AnimBlueprint whose SequencePlayer nodes get re-pointed
5. Run:
   ```bash
   python scripts/ue_remote_exec.py \
     --code "C:/Users/ethan/repos/weirdplace2/scripts/local/retarget_mocap_to_metahuman.py" \
     --mode ExecuteFile
   ```
6. Verify: open the AnimBP preview. If she's T-posing, the chain mapping likely
   didn't take — see "Diagnosing T-pose output" below.

## Reusable assets the script created (don't delete)

- `/Game/MC_Sample/Demo/Characters/MCUE5v2/IK_MCUE5v2` — IK Rig built on the
  mocapcentral skeleton (duplicated from `IK_Mannequin` with its mesh swapped;
  bone names match so chains carry over).
- `/Game/Characters/Mannequins/Rigs/RTG_Mannequin_to_Metahuman` — IK Retargeter,
  source = `IK_MCUE5v2`, target = `IK_metahuman`, with 43 chain mappings set
  including twist chains collapsed onto their parent limbs.

Anything else from the mocapcentral pack? Just edit `SRC_ANIM_PATH` and re-run —
the script reuses both assets above.

## Identifying the source skeleton

A .uasset dropped in by hand often has a skeleton ref that doesn't resolve (the
referenced skeleton was never imported). Symptoms in Python:

```python
anim.get_editor_property('skeleton')  # -> None
```

And `AnimSequence.Skeleton` is **C++ read-only** — Python can't reassign it via
`set_editor_property`. So you can't just point it at `SK_Mannequin`.

To see what skeleton the anim *wants*, query the asset registry's dependency list:

```python
import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
print(list(ar.get_dependencies('/Game/Animations/your_anim',
    unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True))))
```

The non-`/Engine/` entry is the skeleton path. Provide that exact path by
copying the skeleton `.uasset` from the source pack into `Content/...`, then
re-run.

## How the script reloads a broken anim

`AnimSequence` caches `skeleton=None` if it was first loaded before the
referenced skeleton existed. `unreal.load_asset` returns the cached object — so
even after you drop the skeleton `.uasset` in, the anim still reads as broken.

The fix the script applies:

```python
unreal.EditorLoadingAndSavingUtils.reload_packages(
    [anim_pkg], unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
```

Then the next `load_asset` returns a fresh AnimSequence with the skeleton ref
resolved.

## The chain-mapping gotcha — why the FIRST attempt T-poses

`IKRetargeterController.set_ik_rig(SOURCE/TARGET, rig)` is NOT enough. Each
retarget op (Pelvis Motion, FK Chains, IK Chains, Run IK Rig, Root Motion,
Remap Curves) holds its own source/target IK rig refs. Without those:

- `set_source_chain(...)` returns `False` for every chain
- `auto_map_chains(EXACT, force_remap=False)` silently no-ops (no exception)
- The retarget runs successfully but every frame is bind/T-pose

The script fixes this by calling, before any mapping:

```python
ctrl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
ctrl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
```

It then walks every target chain and explicitly sets a source chain — exact
name match, case-insensitive fallback (handles `root`/`Root`, `head`/`Head`),
twists collapsed to their parent limb (`LeftUpperArmTwist01` → `LeftArm`).

## Diagnosing T-pose output

If the AnimBP preview still T-poses after running the script:

```python
import unreal
rtg = unreal.load_asset('/Game/Characters/Mannequins/Rigs/RTG_Mannequin_to_Metahuman')
rc = unreal.IKRetargeterController.get_controller(rtg)
tgt = unreal.IKRigController.get_controller(unreal.load_asset('/Game/MetaHumans/Common/Common/IK_metahuman'))
for c in tgt.get_retarget_chains():
    print(c.chain_name, '<-', rc.get_source_chain(c.chain_name))
```

Any `None` is an unmapped chain. If many are `None`, the
`assign_ik_rig_to_all_ops` calls didn't run (or the source rig has different
chain names than expected and the picker function in `_map_chains` returned
None).

## Python API gotchas worth knowing

- Asset class is `unreal.IKRetargeter`; the factory drops the `er` and is
  `unreal.IKRetargetFactory` (NOT `IKRetargeterFactory`).
- `IKRetargeterController.set_preview_mesh(...)` wants a `SkeletalMesh`, not a
  `Skeleton`. Pass e.g. `SKM_MCUE5v2`, not `SKM_MCUE5v2_Skeleton`.
- `IKRetargetBatchOperation.duplicate_and_retarget(...)` parameter types:
  - `assets_to_retarget`: list of `AssetData` (NOT loaded `AnimSequence`)
    — get via `AssetRegistry.get_asset_by_object_path(anim.get_path_name())`.
  - `source_mesh` / `target_mesh`: `SkeletalMesh` (NOT IK Rigs).
  - Returns `Array[AssetData]`; access `ad.package_name` and `ad.asset_name`.
  - Default destination is `/Game/`, not the source folder. Rename afterwards.
- `BlueprintEditorLibrary.get_all_graphs` doesn't exist. Use
  `BPEL.find_graph(abp, 'AnimGraph')` plus
  `graph.get_graph_nodes_of_class(unreal.AnimGraphNode_SequencePlayer)`.
- To edit a SequencePlayer's sequence ref from Python, mutate the inner
  `FAnimNode_SequencePlayer` struct and write it BACK (structs are value-copied):
  ```python
  inner = sp_node.get_editor_property('node')
  inner.set_editor_property('sequence', new_anim)
  sp_node.set_editor_property('node', inner)
  ```
