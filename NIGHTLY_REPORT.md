# Nightly Report — 2026-06-19

**Branch:** `overnight/2026-06-19` (cut off `overnight/2026-06-17`) · **Nothing merges to `overnight/2026-06-17` until you OK it.** One commit per item = restore point.

Two Claude-Friendly todos, both TDD'd red→green against the E2E harness.

> The previous 2026-06-17 nightly report (Tornado/Telephone beats) is preserved in git history on `overnight/2026-06-17`; this file was overwritten for tonight's run.

---

## Status at a glance

| Item | Todo | Test | Result |
|------|------|------|--------|
| 1 | door lock sound shouldn't play while inserting the key | `Regression.LockSoundDuringKeyInsert` | ✅ green |
| 2 | held/inspected items hard to see in the dark | `Regression.HeldItemDarkGlow` | ⏳ in progress |

---

## Item 1 — Re-entrancy guard on the bathroom-door key-break ✅

**Root cause.** `AOutsideBathroomDoor::StartKeyBreakSequence()` removes the Key + clears the active item *immediately*, but `bDidDropKey` isn't set until the broken-key pickup spawns ~3s later. Any interact in that window hit the `ActiveItem != KeyToRemove` branch and played `LockedDoorSound`. Under the UE5.7 double-fire input quirk a single key-insert press fires twice, so the locked rattle played on a normal insertion.

**Fix.** Added `bKeyBreakInProgress` (armed at the top of `StartKeyBreakSequence`). At the top of `Interact_Implementation`, `if (bKeyBreakInProgress && !bDidDropKey) return;` — ignores the re-entrant interact. No clear needed; once `bDidDropKey` flips the door behaves as a normal locked door again. Single guard branch, no fallback logic.

**Test seam.** `LockedSoundPlayCount` increments in the locked-rattle branch (regardless of whether `LockedDoorSound` is assigned, so RED is genuine); public `GetLockedSoundPlayCount()`. Driver `GetBathroomDoorLockedSoundCount()` + `SetActiveTestItem()`; latent cmds `FTD_AssertBathroomDoorLockedSoundCount` + `FTD_SetActiveItem`.

**TDD.** RED (no guard): `count=1, expected 0` ❌ — confirms the test reproduces the bug. GREEN (guard): `count==0` ✅ (9 steps, headless). **Regression:** `HappyPath` ✅ green (131 steps) — the `UseKeyOnDoor` path is unaffected.

Files: `OutsideBathroomDoor.h/.cpp`, `TestDriverSubsystem.h/.cpp`, `E2E_LatentCommands.h`, `E2E_Level1Test.cpp`.

---

## Item 2 — Self-illuminating held/inspected items ⏳

(In progress — section will be filled on green.)

---

## Verification method
- Editor closed before every `Build.bat weirdplace2Editor Win64 Development`.
- `run_e2e.ps1 -TestName <Name>`; results read from `Saved/Logs/E2ETest.log` scoped to the latest `=== E2E TEST START ===`.
