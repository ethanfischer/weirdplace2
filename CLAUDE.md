# CLAUDE.md
## Project Overview

Weirdplace2 is an Unreal Engine 5.4 first-person exploration game targeting VR 

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
"C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" weirdplace2Editor Win64 Development -Project="C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -WaitMutex -FromMsBuild

# Regenerate project files (if modules/targets change)
"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe" "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -projectfiles

# Run automation tests
"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -ExecCmds="Automation RunTests All; Quit" -unattended -nopause -nosplash -NullRHI
```

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

# Misc
- We modified and used nodetocode to convert blueprints to c++. Modifications are here: https://github.com/protospatial/NodeToCode/pull/14
- This is gonna be a VR game. Implement features diagetically (no screenspace UI)
