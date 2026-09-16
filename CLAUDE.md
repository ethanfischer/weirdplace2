# CLAUDE.md
## Project Overview

Weirdplace2 is an Unreal Engine 5.8 first-person exploration game

## Build Commands

**Always build after making C++ changes.** Do NOT ask the user to build — do it yourself.

**`.cpp`-only changes → Live Coding** (editor stays open):
```bash
powershell -ExecutionPolicy Bypass -File livecode.ps1
```

**Full Restart Required** — header changes (UPROPERTY/UFUNCTION), new classes, changed signatures, `.Build.cs`:
1. Kill the editor: `Stop-Process -Name UnrealEditor -Force` (PowerShell) or `taskkill //F //IM UnrealEditor.exe` (Bash tool; the doubled slashes are MSYS escaping).
2. Build with `Build.bat` (see "Build commands" below; ~15s incremental)
3. Relaunch the editor via `scripts/launch_editor.ps1` (see "Launching the editor" below) — NOT raw `UnrealEditor.exe`, which silently hangs the agent.

## Launching the editor

**Never** invoke `UnrealEditor.exe` directly through the Bash tool with `run_in_background: true`. The editor never exits, so no completion notification ever fires and the agent silently hangs (a previous session lost 9 hours to this). Always launch via `scripts/launch_editor.ps1`, which polls the Python ready-probe and exits as soon as the editor responds READY (or after a 180s timeout).

```powershell
# Foreground (blocks ~5–30s until READY):
powershell -ExecutionPolicy Bypass -File scripts/launch_editor.ps1

# Background (notification fires on READY/timeout, ~5–180s):
# Bash tool with run_in_background: true, same command.

# Headless variant for -ExecutePythonScript workflows:
powershell -ExecutionPolicy Bypass -File scripts/launch_editor.ps1 -Headless
```

If an editor is already running the wrapper prints `EDITOR ALREADY RUNNING` and exits 0 without spawning a duplicate.

Build commands:
```cmd
# Build editor target (typical for C++ changes)
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" weirdplace2Editor Win64 Development -Project="C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -WaitMutex -FromMsBuild

# Regenerate project files (if modules/targets change)
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -projectfiles

# Run automation tests
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -ExecCmds="Automation RunTests All; Quit" -unattended -nopause -nosplash -NullRHI
```

## Task skills

Mocap retargeting → `/retarget-mocap`. Steam Deck deploy → `/steam-deck-deploy`.

## Editor Property Assignment

When adding `UPROPERTY` references to other actors (e.g., `AActor*`, `ADoor*`, `ASeneca*`):
- **Level instance references** (pointing to actors placed in the level) must be assigned on the **level instance** in the viewport Details panel, NOT in the Blueprint class defaults. The Blueprint editor cannot see level-placed actors.
- **Asset references** (pointing to meshes, materials, sounds, dialogue assets, classes) can be assigned in either the Blueprint class defaults or the level instance.
- Always tell the user which properties need to be set on the **level instance** vs **Blueprint defaults**.

## Code Conventions

- **Never name C++ classes with `BP_` or `BPI_` prefix** - those stand for Blueprint/BlueprintInterface
- **Interface naming**: UINTERFACE is `UInteractable`, interface class is `IInteractable`, file is `Interactable.h`
- Forward declarations in headers; heavy includes only in .cpp
- **Tunable gameplay constants**: don't hardcode magic numbers you (or the user) will want to dial in — declare them with `WP_TUNABLE_FLOAT/INT/BOOL` from `Tunable.h` (cvar prefix `weird.<System>.<Name>`), then tune live via the unreal-mcp cvar tools (EditorToolset; `uq cvar` as fallback) and bake the final value back into the default. New tunables need a full editor restart to register (Live Coding won't); tweaking existing ones is always live. See docs/dev-tooling.md.

## Dev tooling — read docs/dev-tooling.md before writing a one-off script

- **unreal-mcp** — default for live-editor work (actors, properties, cvars, screenshots, PIE). If it shows as failed to connect, launch the editor then run `/mcp`; don't silently fall back to uq.
- **TestDriverToolset** (MCP) — drive the game live in PIE: batched input sequences, completion-based waits, teleports, `CapturePlayerView` for a first-person screenshot. Dial a flow in live, then transcribe it to an E2E test.
- **`scripts/uq.py`** — fallback when the editor/MCP is down; only path for `mat-params`, `refs`, `save`, arbitrary editor Python. Extend uq with a verb rather than adding a `scripts/local/*.py` one-off.
- **`scripts/ue_remote_exec.py --code <ABSOLUTE path> --mode ExecuteFile`** — arbitrary live-editor Python. Never `--file` (silently no-ops).
- **`scripts/logq.py`** — log triage (`--e2e` for the test log). Prefer over hand-rolled grep.
- **`scripts/dq.py lint`** — run after any dialogue edit (`Content/Dialogue/`).
- **`scripts/sfx.py`** — search here first when asked to add a sound (`/ftus-sfx`).
- **`scripts/e2e_report.py`** — screenshot goldens diff + gallery; `--bless` to accept.

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
Prefer `python scripts/logq.py` (see Dev tooling above) for triage; raw grep below for targeted digs.

The active log is at:
```
C:\Users\ethan\repos\weirdplace2\Saved\Logs\weirdplace2.log
```

Use `grep` to search for relevant lines:
```bash
grep -n "MyKeyword\|OtherKeyword" "C:/Users/ethan/repos/weirdplace2/Saved/Logs/weirdplace2.log" | tail -80
```

## E2E Testing

Tests live in `E2E_Level1Test.cpp`. Use screenshots in tests so you can check visually as well as via logs and pass/fail. **Run tests yourself and verify the screenshots. Don't hand the run-and-verify step back to the user.**

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

## Misc
- This is gonna be a VR game. Implement features diegetically (no screenspace UI)
- If you add 3rd party assets, credit them in `CREDITS.md`
- Do not ask me to run python scripts for you. No "Run this in UE's Output Log:". Run them yourself (see Dev tooling).

## Agent skills

### Issue tracker

Issues are tracked on the Trello board "weirdplace" (https://trello.com/b/apYW69HZ/weirdplace) via the Trello MCP tools. Green label = agent queue. See `docs/agents/issue-tracker.md` for IDs and conventions.

### Domain docs

Single-context: `CONTEXT.md` + `docs/adr/` at the repo root (created lazily). See `docs/agents/domain.md`.

## Ending your turn
If you've made changes that I need to manually verify, launch the editor via `scripts/launch_editor.ps1` so it's ready to go when I return to my computer
