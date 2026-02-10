# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Weirdplace2 is an Unreal Engine 5.4 first-person exploration game centered around collecting randomized VHS boxes with procedurally-generated cover art. Windows is the primary platform.

**Required Plugin Setup:** Clone DlgSystem into Plugins folder before opening:
```
cd Plugins
git clone -b UpgradeTo5.1 https://github.com/NotYetGames/DlgSystem.git
```

## Build Commands

**Live Coding (Ctrl+Alt+F11 in UE Editor)** - use for:
- `.cpp` implementation changes (function body edits only)

**Full Restart Required** - close UE, rebuild from Rider/VS:
- Adding/removing `UPROPERTY` or `UFUNCTION`
- Changing function signatures in headers
- Adding new classes or files
- Changing class inheritance
- Modifying `.Build.cs`

**Always tell the user which type of rebuild is needed after making changes.**

**Always build after making C++ changes** to verify they compile before telling the user you're done.

If the change requires a **Full Restart** (header changes with UPROPERTY/UFUNCTION, new classes, etc.), ask the user to close the editor first and wait for confirmation before building.

Build commands:
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
- Owns `UInventoryComponent` (tracks collected items via `EInventoryItem` enum)
- Owns `UInventoryRoomComponent` (teleports player to separate space to view inventory)
- Input: Tab toggles inventory room

**Interaction System**
- `IInteractable` - C++ interface (NotBlueprintable)
- `AMovieBox` - Main collectible implementing IInteractable; handles inspection mode where player rotates box in front of camera
- Dynamically loads VHS cover materials: `MI_VHSCover_<TITLE>` based on actor name

**Inventory System**
- `UInventoryComponent` - Manages collected items with `OnInventoryChanged` delegate
- `UInventoryRoomComponent` - Teleports player, spawns 3D display actors in grid layout
- `AMovieBoxDisplayActor` - Displays collected covers with matching materials

**Spawning System**
- `USpawnerActorComponent` - Procedurally spawns MovieBox instances from DataTable
- Names spawned boxes as `<DataTableRowName>_<Index>`; MovieBox strips suffix to load correct cover

### VHS Cover Pipeline

Python scripts in `Content/Python/` drive material generation:
1. `optimize_vhs_textures.py` - Caps textures at 512px, enables streaming
2. `create_vhs_material_instances.py` - Generates MIs from textures

Run in UE Output Log: `py "Content/Python/optimize_vhs_textures.py"`

### Input Bindings (DefaultInput.ini)

- `Interact` (E) - Interact with objects
- `Exit Interaction` (Q) - Exit inspection mode
- `Collect Inspected Subitem` (E/SpaceBar) - Collect from inspected box
- `ToggleInventory` (Tab) - Toggle inventory room
- `Turn Right / Left Mouse/Gamepad` - Rotate inspected actor

## Code Conventions

- **Never name C++ classes with `BP_` or `BPI_` prefix** - those stand for Blueprint/BlueprintInterface
- **Interface naming**: UINTERFACE is `UInteractable`, interface class is `IInteractable`, file is `Interactable.h`
- Forward declarations in headers; heavy includes only in .cpp
- `GENERATED_BODY()` first inside UCLASS/USTRUCT
- Use `TArray`, `TMap`, `TSet` (not STL) for reflection/GC compatibility
- Mark UObject pointers with `UPROPERTY()` to prevent GC collection
- Use `CreateDefaultSubobject` for owned components in constructors
- Null-check pointers before dereference; early-return on failure
- Use `UE_LOG(LogTemp, ...)` for debugging

## Windows Git Performance

Live Coding uses `git status` for adaptive unity builds. Enable fsmonitor to reduce from ~65s to ~70ms:

```cmd
git config core.fsmonitor true
git config core.untrackedcache true
```

**Important:** Run git commands from Windows (not WSL). WSL git invalidates the Windows cache.

## macOS / Xcode Compatibility

On macOS 26 with Xcode 26.x, edit `Engine/Config/Apple/Apple_SDK.json` and change `MaxVersion` from `"16.9.0"` to `"26.9.0"`.

## Feature Documentation

**Always read `features.md` when:**
- The user asks about how a feature works
- Before modifying an existing feature
- To understand the current implementation of a system

**After completing a feature**, document it in `features.md` with:
- Feature name
- Key files/classes
- High-level behavior and configuration options

We modified and used nodetocode to convert blueprints to c++. Modifications are here:
https://github.com/protospatial/NodeToCode/pull/14
