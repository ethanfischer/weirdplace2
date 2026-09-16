# Dev tooling

Tools that replace the old one-off-script workflow. All Python scripts run with
the system `python` (3.13); `e2e_report.py` needs Pillow + numpy (`pip install --user pillow numpy`).

## unreal-mcp — default for live-editor work

The Epic `UnrealMCP` plugin hosts an MCP server inside the editor (`.mcp.json`,
`http://127.0.0.1:8000/mcp`). It is 8-180x faster per call than uq (persistent session
vs ~780ms interpreter+discovery per CLI call; A/B'd 2026-08-07 with
`scripts/local/ab_mcp_vs_uq.py`). Use it for finding/inspecting/modifying actors,
properties, cvars, screenshots, PIE control.

**If the server shows as failed to connect**, it's because no editor was running when
the session started. Launch via `scripts/launch_editor.ps1`, then run `/mcp` to
reconnect. Don't silently fall back to uq.

Discovery: `list_toolsets` → `describe_toolset` → `call_tool` (tool_name WITHOUT the
toolset prefix, toolset_name separate; schema-"optional" params must still be passed,
as null). Beyond actor/property basics:

- **LogsToolset** — read the live editor log, set category verbosity.
- **AutomationTestToolset** — run automation tests in the live editor; handy for quick
  single-test iteration. `run_e2e.ps1` remains the gate since it's a fresh process
  loading the on-disk DLL.
- **SemanticSearchToolset** — hybrid vector+BM25 asset search.
- **SlateInspectorToolset** — drive/inspect editor UI, Playwright-style.
- **EditorToolset.EditorAppToolset** — StartPIE/StopPIE.

### TestDriverToolset — drive the game live in PIE

Project toolset (`Source/weirdplace2Editor/TestDriverToolset.h`, registered as
`weirdplace2Editor.TestDriverToolset`) wrapping `UTestDriverSubsystem` for interactive
feature-driving without writing an E2E test. Dial in a flow live first, then transcribe
it into an E2E test — the latent commands map 1:1.

- **Batch, don't per-keypress**: `PressInputSequence(["Settings","NextOption","NextOption","NextOption","Interact"], 0.3)`
  plays a whole navigation in one async call.
- **Completion-based waits**: `WaitForActivityState` / `WaitForMenuPage` — use instead
  of sleeps; they error on timeout with the current state.
- Singles: `PressInputAction` (Interact/Inventory/Settings/NextOption/PreviousOption/NavigateLeft/NavigateRight/Back),
  `SetInputActionPressed` (holds), `PressKey`.
- Queries/movement: `GetPlayerStatus` (activity state + menu page + inventory + location;
  enum names match the Wait tools), `TeleportToWaypoint`, `TeleportNearActor`, `LookAtActor`.
- **PIE-view screenshot**: `CapturePlayerView(maxDimension)` captures the first-person
  game view (diegetic UI in, editor chrome out), writes `Saved/Screenshots/PlayerView.png`
  and returns the path; Read that path. It deliberately returns a path, NOT inline
  base64: this MCP server serializes images as base64 in a JSON text field, which blows
  the tool-result token cap (~60K chars even at 320px, measured). The engine's own
  `CaptureViewport`/`CaptureEditorImage` hit the same wall, and `CaptureViewport` shows
  the EDITOR camera, not the PIE view.
- When MCP is down, the same methods are reachable via remote-exec reflection:
  `unreal.TestDriverToolset.get_default_object().call_method('PressInputAction', ('Settings',))`
  (read returned structs with `.export_text()`).

## Arbitrary editor Python

For standard live-editor operations prefer unreal-mcp (above). Two paths for arbitrary Python:

**Live editor** (in-memory state, current viewport, selection) — Python Remote Execution,
already enabled in Project Settings → Plugins → Python. `scripts/ue_remote_exec.py`
discovers the editor via UDP multicast (239.0.0.1:6766) and prints whatever the script printed.

```bash
# File: must be an ABSOLUTE path — MODE_EXEC_FILE resolves it directly
python scripts/ue_remote_exec.py --code "C:/Users/ethan/repos/weirdplace2/Content/Python/your_script.py" --mode ExecuteFile

# Inline
python scripts/ue_remote_exec.py --code "import unreal; print(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name())"
```

Do not use `--file <path>`: the wrapper ships file contents, but MODE_EXEC_FILE expects
a path and the run silently fails with empty output (verified on 5.7; unchanged in 5.8).

**Headless / asset modification** (edit `.uasset` files without the user's session) —
`UnrealEditor-Cmd.exe -ExecutePythonScript=...`. For bulk asset edits, thumbnails,
batch processing. Does NOT see live editor state.

## uq — query/command the live editor (`scripts/uq.py`)

> **2026-08-07: prefer the unreal-mcp server for live-editor work** — same operations,
> 8-180x faster per call (persistent HTTP session vs uq's per-call interpreter start +
> UDP discovery; benchmarked with `scripts/local/ab_mcp_vs_uq.py`, 0 failures both sides).
> uq remains the fallback when the editor/MCP is down and the only path for `mat-params`,
> `refs`, `save`, and arbitrary editor Python (`py`/`pyfile`).

Talks to the running editor via Python Remote Execution. **Reach for this before
writing a new `scripts/local/*.py` script** — most one-off needs are a uq verb.

```bash
python scripts/uq.py actors Seneca              # find actors (label/name substring)
python scripts/uq.py actors --cls Door          # filter by class
python scripts/uq.py sel                        # selected actors
python scripts/uq.py components BP_Rick2
python scripts/uq.py props BP_Rick2 --match material
python scripts/uq.py props BP_Rick2/Body        # component target: Actor/Component
python scripts/uq.py props /Game/Blueprints/BP_TV       # asset
python scripts/uq.py props cdo:/Game/Blueprints/BP_TV   # Blueprint class defaults (CDO)
python scripts/uq.py get domelight/LightComponent0 intensity
python scripts/uq.py set domelight/LightComponent0 intensity 2500
python scripts/uq.py bounds MovieShelf          # WARNING: stale right after set_static_mesh
python scripts/uq.py screenshot --name look.png --res 1920x1080   # prints file path
python scripts/uq.py mat-params /Game/CreatedMaterials/M_VHSCover # names repr'd (trailing-space params visible)
python scripts/uq.py assets /Game/VHSCovers --cls Texture2D
python scripts/uq.py refs /Game/Blueprints/BP_TV        # referencers (--deps for dependencies)
python scripts/uq.py cvar weird.CarRide.Speed 900       # set cvar; bare name reads it
python scripts/uq.py cvar --dump                # weird.Tunables: all project tunables
python scripts/uq.py exec "stat fps"            # console cmd (game world if PIE active)
python scripts/uq.py py "print(unreal.__file__)"        # inline python passthrough
python scripts/uq.py save                       # save dirty packages
```

Notes:
- `set` coerces from the current value's type (float/int/bool/str/Name/Text/enum,
  Vector/Rotator/LinearColor as `x,y,z`), calls `modify()`, and is **in-memory
  only** until `uq save`.
- Actor lookup: exact label/name wins, else unique substring; ambiguity errors list candidates.
- `/Game/...` args survive Git Bash MSYS path mangling (auto-repaired).
- Add `--json` for machine-readable output.
- If no editor is running you get a clear error (exit 2); launch via `scripts/launch_editor.ps1`.

## logq — log triage (`scripts/logq.py`)

Dedupe + error/warning summary, scoped to a session instead of the whole file.

```bash
python scripts/logq.py                    # editor log, LATEST PIE session
python scripts/logq.py --session -2       # earlier session; --sessions lists them
python scripts/logq.py --all              # whole editor log
python scripts/logq.py --e2e              # E2ETest.log: per-test error counts + summary
python scripts/logq.py --test HappyPath   # one E2E test's scope
python scripts/logq.py --grep "Seneca|Bladder"   # raw matching lines in scope
```

Errors first, then warnings, deduped with counts. `LogAutomationController: ... [log]`
echo lines are unwrapped so nothing double-counts. Exit 1 if the scope contains errors.

## dq — dialogue lint + preview (`scripts/dq.py`)

Dialogue lives in one sectioned file per NPC under `Content/Dialogue/`
(`Seneca.txt`, `Rick.txt`, `Hudson.txt`), parsed at runtime by the shared
`FDialogueScript` (`Source/weirdplace2/DialogueScript.h`). Format:
`== SectionName ==` headers, `# comment` lines, optional single-word
`Speaker:` prefix (default = the NPC owning the file), `[Tag]` lines that
attach an action to the preceding line. `MovieComments.txt` stays a keyed
`KEY: line one|line two` table.

```bash
python scripts/dq.py lint                 # all files + Source/ cross-ref; exit 1 on error
python scripts/dq.py lint --file <path>   # single file, per-file rules only
python scripts/dq.py preview              # write + open Saved/DialoguePreview/preview.html
```

Lint errors: unknown/misplaced `[Tag]` (exact-match table in dq.py — extend it
when adding a tag), tag with no preceding line, unknown speaker, duplicate
sections, sections not referenced from `Source/` (and vice versa), malformed
MovieComments rows. Warns when a line would wrap to 3+ rows on the 700-unit
dialogue plate. The previewer replays lines with the runtime typewriter timing
so dialogue can be written without playing the game.

## e2e_report — screenshot goldens + gallery (`scripts/e2e_report.py`)

Compares `Saved/Screenshots/**` against `Tests/E2EGoldens/` (committed, LFS) and
writes a one-page gallery to `Saved/E2EReport/report.html`.

```bash
python scripts/e2e_report.py                    # compare E2E_* shots; exit 1 on DIFF/BAD
python scripts/e2e_report.py --bless            # accept ALL current shots as goldens
python scripts/e2e_report.py --bless E2E_Poster_01_Pole.png
python scripts/e2e_report.py --pattern "Diag_*" --threshold 5
powershell -ExecutionPolicy Bypass -File run_e2e.ps1 -TestName Regression -Headed -Report
```

Verdicts: `PASS` (≤ threshold % changed pixels, default 2%), `DIFF` (heatmap PNG
generated next to the report), `NEW` (no golden yet — bless once it looks right),
`BAD` (unreadable/zero-byte, i.e. a NullRHI run; use `-Headed`).
Workflow: after an intentional visual change, eyeball the gallery, then `--bless`
the changed shots so the new look becomes the baseline (bless shows up in git).

## WP_TUNABLE — live-tunable constants (`Source/weirdplace2/Tunable.h`)

For gameplay constants you expect to dial in. Do NOT hardcode a magic number you'll
want to tweak — declare it as a tunable cvar:

```cpp
#include "Tunable.h"
WP_TUNABLE_FLOAT(GHeadlightIntensity, "weird.Headlight.Intensity", 45000.f,
    "Car headlight intensity in lumens.");
```

Loop: play PIE → `uq cvar weird.Headlight.Intensity 60000` (applies same frame,
survives PIE stop/start) → when dialed, `uq cvar --dump` lists every tunable with
`*` on the ones changed this session → bake those numbers back into the defaults.

- Prefix is `weird.<System>.<Name>` (`wp.` is taken by engine World Partition).
- Registration runs in static initializers: a **new** tunable needs a full editor
  restart (Live Coding won't register it); tweaks to existing ones are always live.
- `WP_TUNABLE_INT` / `WP_TUNABLE_BOOL` also exist. `weird.Tunables` is the in-editor
  console equivalent of `uq cvar --dump`.
- Worked example: `weird.CarRide.Speed` in `CarRideComponent.cpp`.
