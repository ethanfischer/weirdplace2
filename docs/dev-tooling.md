# Dev tooling

Four tools that replace the old one-off-script workflow. All Python scripts run with
the system `python` (3.13); `e2e_report.py` needs Pillow + numpy (`pip install --user pillow numpy`).

## uq — query/command the live editor (`scripts/uq.py`)

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
