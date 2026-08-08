# E2E Harness Recipe

Everything the overnight loop needs to build, test, screenshot, and read results
without asking the user. Paths assume repo root `C:/Users/ethan/Repos/weirdplace2`.

## The iteration cycle

For E2E work the cycle is **edit code → full build → run_e2e → read log → read
screenshot**. The editor must be **closed** (`taskkill //F //IM UnrealEditor.exe`)
so the build can link the editor DLL.

### Why not Live Coding
Live Coding patches the *running editor's* memory. `run_e2e.ps1` spawns a **fresh
`UnrealEditor-Cmd.exe`** that loads the on-disk DLL, so it never sees a Live Coding
patch. Every E2E iteration therefore needs a real `Build.bat` link, not livecode.

### Build (editor target)
```cmd
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" weirdplace2Editor Win64 Development -Project="C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -WaitMutex -FromMsBuild
```
Run it via the Bash tool. If you added a new class, `UPROPERTY`/`UFUNCTION`, or
changed a signature, this full build is mandatory (header changes never reach a
running editor anyway).

### Run an E2E test
```bash
powershell -ExecutionPolicy Bypass -File run_e2e.ps1 -TestName <Name> -Headed
```
- Always pass **`-Headed`**. The default (`-NullRHI`) is faster but renders
  nothing, so screenshots come out blank/zero-byte — useless for visual checks.
- Run it with `run_in_background: true`; a test is 2–5 minutes. You'll be notified
  when it finishes — don't poll in a sleep loop.
- Output is terse: `PASS`/`FAIL` plus errors. `<Name>` maps to the test path
  `Weirdplace2.E2E.Level1.<Name>`.

### Read the result log
Log file: `Saved/Logs/E2ETest.log` (separate from the editor's own log). Never Read
the whole thing — grep it, scoped to the latest run marker:
```bash
grep -n "=== E2E TEST START ===\|Test Completed\|Error\|AddError\|TestDriver::Status" "C:/Users/ethan/Repos/weirdplace2/Saved/Logs/E2ETest.log" | tail -60
```
Each run begins with `=== E2E TEST START === <Label>`; only trust lines after the
*last* such marker (earlier ones are stale).

### Read screenshots
They land in `Saved/Screenshots/Windows/`. Open them with the **Read** tool and
actually look. A zero-byte or all-black image means the run was headless — re-run
with `-Headed`.

## Writing a test

Add a new automation test to `Source/weirdplace2/E2E_Level1Test.cpp`:
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FE2E_Level1_<Name>,
    "Weirdplace2.E2E.Level1.<Name>",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_<Name>::RunTest(const FString& Parameters)
{
    E2E_TEST_PREAMBLE("<Name>")          // logs the START marker, opens the map, waits for player

    ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNear...(this));
    ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAt...(this));
    ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
    ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_<slug>_AtMomentOfTruth")));
    ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertHasItem(this, FName("...")));   // the spec

    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
```
- Reusable multi-step flows live as helpers in `E2E_Steps.h`; the latent commands
  (`FTD_*`) are declared in `E2E_LatentCommands.h`. Grep that header for the full
  catalog before inventing a new one — most building blocks already exist.
- Screenshot-naming: `E2E_<itemSlug>_<step>` keeps a run's shots grouped and easy
  to find in the morning.
- If a test expects engine-side warnings (e.g. JPEG/texture decode noise), guard
  them with `AddExpectedError(...)` as the existing tests do, so they don't fail
  the run spuriously.

### FTD_* building blocks (catalog in E2E_LatentCommands.h)
Movement/aim: `FTD_TeleportNear{Seneca,Rick,Hudson,BlankTape,Actor ByLabel}`,
`FTD_TeleportTo`, `FTD_LookAt{Seneca,Rick,Hudson,...,ActorByLabel,Waypoint}`,
`FTD_LerpTo`.
Input: `FTD_SimulateInteractAction`, `FTD_SimulateInventoryAction`,
`FTD_SimulateNavAction`, `FTD_OpenInventoryViaInput`, `FTD_AdvanceDialogueViaInput`.
Waits: `FTD_WaitForPlayerReady`, `FTD_WaitForActivityState`, `FTD_WaitForItemAdded`,
`FTD_WaitForDoorOpen`, `FTD_Delay`.
Asserts: `FTD_AssertHasItem`, `FTD_AssertNotHasItem`, `FTD_AssertInventoryCount`,
`FTD_AssertActivityState`, `FTD_AssertMenuPage`, `FTD_AssertYawDelta`.
Setup/util: `FTD_UnlockInventory`, `FTD_AddTestItem`, `FTD_TakeScreenshot`.

## Known 5.7 gotchas (don't rediscover these at 3am)

- **Simulated key input poisons `IA_Interact`.** `SimulateKeyPress(E)` breaks the
  collect/interact DoOnce under 5.7. Drive interactions through the dedicated
  `FTD_SimulateInteractAction` / `TestDriverSubsystem` triggers, not raw key
  presses. (Memory: `feedback_ue57_simulated_input_consume`.)
- **Inventory open is gated by Seneca's intro.** `bInventoryUnlocked` is false
  until the intro fires; a focused test that opens inventory must call
  `FTD_UnlockInventory` first or it times out. (Memory:
  `project_inventory_unlock_gate`.)
- **Teleporting a Character floor-snaps.** Disable movement (MOVE_None +
  StopMovementImmediately) *before* the teleport and use
  `ETeleportType::TeleportPhysics`, or 5.7 snaps it next tick. The existing
  `FTD_TeleportNear*` commands already handle this — prefer them.
  (Memory: `feedback_ue57_setactor_floor_snap`.)
- **Latent commands construct up-front, tick in order.** Use
  `GetElapsedSinceFirstTick`, and remember `AddError` does *not* abort the queue.
  (Memory: `feedback_latent_command_timing`.)
- **`set_editor_property` can't write UPROPERTYs on Blueprint-class level actors**
  via headless Python; design Blueprint-touching changes around the CDO/ICH
  patterns or accept a manual Details-panel set (note it in the report).
  (Memory: `reference_bp_cdo_component_edit`, `reference_ich_override_inherited_smc`,
  `feedback_set_editor_property_templates`.)

When a build or test misbehaves in a way not covered here, the project memory
index (`MEMORY.md`) almost certainly has the specific gotcha — check it before
burning the night on a known issue.
