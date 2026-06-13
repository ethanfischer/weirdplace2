# CLAUDE.md
## Project Overview

Weirdplace2 is an Unreal Engine 5.7 first-person exploration game targeting VR 

## Build Commands

**Always build after making C++ changes.** Do NOT ask the user to build — do it yourself.

**`.cpp`-only changes → Live Coding** (editor stays open):
```bash
powershell -ExecutionPolicy Bypass -File livecode.ps1
```

**Full Restart Required** — header changes (UPROPERTY/UFUNCTION), new classes, changed signatures, `.Build.cs`:
1. `taskkill //F //IM UnrealEditor.exe` (Note: `//F //IM` is Windows CMD syntax; run from a CMD terminal if Git Bash rejects the flags.)
2. `mcp__jetbrains__build_project`
3. `mcp__jetbrains__execute_run_configuration` with `configurationName: "weirdplace2"`

Build commands (fallback if MCP is unavailable):
```cmd
# Build editor target (typical for C++ changes)
"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" weirdplace2Editor Win64 Development -Project="C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -WaitMutex -FromMsBuild

# Regenerate project files (if modules/targets change)
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -projectfiles

# Run automation tests
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -ExecCmds="Automation RunTests All; Quit" -unattended -nopause -nosplash -NullRHI
```

## Retargeting Mocap Animations to MetaHumans

`scripts/local/retarget_mocap_to_metahuman.py` retargets a UE5-Mannequin-skeleton
anim (e.g. mocapcentral) onto a MetaHuman and swaps the matching SequencePlayer
nodes in a target AnimBlueprint. Idempotent. Edit the constants at the top for a
new animation. Full workflow + Python API gotchas: `docs/animation-retargeting.md`.

## Steam Deck Deploy

Build → `scripts/push_to_deck.ps1 -DeckHost deck@<ip>` → launch from Steam on the Deck (not directly — Steam Input has to wrap the process for the controller to work).

Build flags depend on what changed:
- New `UPROPERTY`/class → full `-cook -allmaps -build -stage` (or you'll hit `Bad export index` on cooked Blueprints)
- `.cpp` only → `-build -skipcook -stage`
- `.ini` only → `-skipbuild -skipcook -stage`

Full command + setup + log retrieval + Deck device profile notes: `docs/steamdeck-deploy.md`.

## Architecture

### Core Systems

**Player Character (`AMyCharacter`)**
**Interaction System**
**Inventory System**

## Editor Property Assignment

When adding `UPROPERTY` references to other actors (e.g., `AActor*`, `ADoor*`, `ASeneca*`):
- **Level instance references** (pointing to actors placed in the level) must be assigned on the **level instance** in the viewport Details panel, NOT in the Blueprint class defaults. The Blueprint editor cannot see level-placed actors.
- **Asset references** (pointing to meshes, materials, sounds, dialogue assets, classes) can be assigned in either the Blueprint class defaults or the level instance.
- Always tell the user which properties need to be set on the **level instance** vs **Blueprint defaults**.

## Code Conventions

- **Never name C++ classes with `BP_` or `BPI_` prefix** - those stand for Blueprint/BlueprintInterface
- **Interface naming**: UINTERFACE is `UInteractable`, interface class is `IInteractable`, file is `Interactable.h`
- Forward declarations in headers; heavy includes only in .cpp
- `GENERATED_BODY()` first inside UCLASS/USTRUCT
- `#include "ClassName.generated.h"` must be the **last** `#include` in every header — UHT enforces this and will error if any include follows it
- Use `TArray`, `TMap`, `TSet` (not STL) for reflection/GC compatibility
- Mark UObject pointers with `UPROPERTY()` to prevent GC collection
- Use `CreateDefaultSubobject` for owned components in constructors
- Null-check pointers before dereference; early-return on failure
- Use `UE_LOG(LogTemp, ...)` for debugging

## Hiding Actors at Runtime

**Do NOT use `SetActorHiddenInGame`** — it sets a flag on the actor but the component's own `bVisible` takes precedence and the mesh stays visible.

Use `SetVisibility` on the root component instead:
```cpp
// Requires: #include "Components/SceneComponent.h"
if (USceneComponent* Root = Actor->GetRootComponent())
{
    Root->SetVisibility(false, true); // false=hide, true=propagate to children
}
```

Setting "Hidden in Game" in the editor Details panel is also unreliable — always enforce visibility state in C++.

## Reading Output Logs

Always read logs directly — never ask the user to copy-paste them.

The active log is at:
```
C:\Users\ethan\repos\weirdplace2\Saved\Logs\weirdplace2.log
```

Use `grep` to search for relevant lines:
```bash
grep -n "MyKeyword\|OtherKeyword" "C:/Users/ethan/repos/weirdplace2/Saved/Logs/weirdplace2.log" | tail -80
```

## E2E Testing

When implementing a new feature or modifying an existing one, ask me if you should write a new E2E test in E2E_Level1Test.cpp to verify your work. Make use of screenshots in the test so you can check your work visually in addition to logs and test passing/failing.

**You are responsible for running the test yourself and verifying screenshots after writing it. Don't hand the run-and-verify step back to the user.**

Run E2E tests with `run_e2e.ps1` (uses a separate log file so it works while the editor is open):
```bash
powershell -ExecutionPolicy Bypass -File run_e2e.ps1                          # HappyPath, headless (NullRHI)
powershell -ExecutionPolicy Bypass -File run_e2e.ps1 -TestName DialogueCooldown
powershell -ExecutionPolicy Bypass -File run_e2e.ps1 -TestName PauseMenu -Headed   # render so screenshots aren't blank
```

**Headed vs headless:** the default `-NullRHI` mode is fast but produces blank/zero-byte screenshots because nothing is rendered. When the test takes screenshots that you intend to inspect visually, pass `-Headed` to run with rendering enabled.

Screenshots land in `Saved/Screenshots/Windows/` (or the platform-specific subdir). Read them with the Read tool to verify the feature looks right.

Output is concise: `PASS` or `FAIL` + any errors. Run with `run_in_background` since tests take 2-5 minutes.

The test log is at `Saved/Logs/E2ETest.log`. To dig into failures:
```bash
grep -n "Error\|AddError\|TestDriver::Status" "C:/Users/ethan/repos/weirdplace2/Saved/Logs/E2ETest.log" | tail -40
```

### Regression vs Diagnostic

Tests in `E2E_Level1Test.cpp` live under two subgroups:

- **`Weirdplace2.E2E.Level1.Regression.*`** — real guards. Failure means something broke.
- **`Weirdplace2.E2E.Level1.Diagnostic.*`** — authoring/inspection tours, loose or no asserts. Not part of the gate; run on demand.

`run_e2e.ps1` defaults a bare `-TestName <Name>` to the Regression subgroup, so existing invocations (`-TestName HappyPath`, `-TestName PauseMenu -Headed`) keep working. To target a diagnostic explicitly: `-TestName Diagnostic.BlankVhsGazeSweep`.

**When finishing a feature, run the regression suite to make sure nothing broke:**
```bash
powershell -ExecutionPolicy Bypass -File run_e2e.ps1 -TestName Regression -Headed -TimeoutMinutes 60
```
Headed because several regression tests (PauseMenu, InventoryThumbnails, GazeReward, MoviePutBackPrompt) take screenshots and/or rely on rendering for trace/material side effects. The script auto-bumps the default timeout to 60 min when the full Regression suite is selected.

# Misc
- We modified and used nodetocode to convert blueprints to c++. Modifications are here: https://github.com/protospatial/NodeToCode/pull/14
- This is gonna be a VR game. Implement features diagetically (no screenspace UI)
- Doors are 110x215
- If you add 3rd party assets, make sure to give them credit in credits.md

## Running Python in UE

Do not ask me to run python scripts for you. No "Run this in UE's Output Log:". You are capable of running python scripts for me.

Two paths depending on what you need:

**Live editor (sees in-memory state, current viewport, selected actors, etc.)** — use Python Remote Execution. Already enabled in Project Settings → Plugins → Python. Wrapper script:
```bash
# File (must be an ABSOLUTE path — UE 5.7's MODE_EXEC_FILE resolves it directly):
python scripts/ue_remote_exec.py --code "C:/Users/ethan/repos/weirdplace2/Content/Python/your_script.py" --mode ExecuteFile

# Inline:
python scripts/ue_remote_exec.py --code "import unreal; print(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name())"
```
Do not use `--file <path>` — the wrapper ships file contents, but 5.7's MODE_EXEC_FILE expects a path and the run silently fails with empty output. Always pass the absolute path through `--code`.
`scripts/ue_remote_exec.py` discovers the editor via UDP multicast (239.0.0.1:6766) and prints whatever the script printed. Use this for: querying the level, listing actors, deleting/moving actors, modifying selected actors. Live state — no save required.

**Headless / asset-modification scripts (modify .uasset files without the user's session)** — invoke `UnrealEditor-Cmd.exe -ExecutePythonScript=...`. Use this for: bulk asset edits, generating thumbnails, batch processing. Does NOT see the user's live editor state.
