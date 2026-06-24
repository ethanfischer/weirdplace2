# Nightly Report — 2026-06-19

**Branch:** `overnight/2026-06-19` (cut off `overnight/2026-06-17`) · **Nothing merges to `overnight/2026-06-17` until you OK it.** One commit per item = restore point.

Two Claude-Friendly todos, both TDD'd red→green against the E2E harness. **Both green, one commit each. Final tip certified: full `Regression` suite green — 17/17 tests, 316 steps** (the 2 new regression tests + 15 pre-existing, all passing together).

> The previous 2026-06-17 nightly report (Tornado/Telephone beats) is preserved in git history on `overnight/2026-06-17`; this file was overwritten for tonight's run.

**Follow-ups (morning 2026-06-20, your requests):** extended the same `M_ItemDarkGlow` glow so it follows the key end to end —
1. *During the door-lock insert animation* (the door's `AnimKeyMesh`): RED→GREEN on `LockSoundDuringKeyInsert` (new `FTD_AssertBathroomDoorAnimKeyGlow`); proof `E2E_KeyInsert_Glow.png` — warm key glowing inside the dark keyhole.
2. *On the dropped broken-key pickup, on the ground* — previously it only glowed once you were inspecting it, so it was invisible on the dark floor. `AInspectablePickup` now applies the overlay from spawn (BeginPlay) instead of on inspect, so it's findable where it lands. RED→GREEN (`FTD_AssertInspectablePickupGlow`); proof `E2E_BrokenKey_GroundGlow.png` — warm glow on the floor at the door base.

(See Commits section below.)

---

## Status at a glance

| Item | Todo | Test | Result |
|------|------|------|--------|
| 1 | door lock sound shouldn't play while inserting the key | `Regression.LockSoundDuringKeyInsert` | ✅ green |
| 2 | held/inspected items hard to see in the dark | `Regression.HeldItemDarkGlow` | ✅ green |

---

## Item 1 — Re-entrancy guard on the bathroom-door key-break ✅

**Root cause.** `AOutsideBathroomDoor::StartKeyBreakSequence()` removes the Key + clears the active item *immediately*, but `bDidDropKey` isn't set until the broken-key pickup spawns ~3s later. Any interact in that window hit the `ActiveItem != KeyToRemove` branch and played `LockedDoorSound`. Under the UE5.7 double-fire input quirk a single key-insert press fires twice, so the locked rattle played on a normal insertion.

**Fix.** Added `bKeyBreakInProgress` (armed at the top of `StartKeyBreakSequence`). At the top of `Interact_Implementation`, `if (bKeyBreakInProgress && !bDidDropKey) return;` — ignores the re-entrant interact. No clear needed; once `bDidDropKey` flips the door behaves as a normal locked door again. Single guard branch, no fallback logic.

**Test seam.** `LockedSoundPlayCount` increments in the locked-rattle branch (regardless of whether `LockedDoorSound` is assigned, so RED is genuine); public `GetLockedSoundPlayCount()`. Driver `GetBathroomDoorLockedSoundCount()` + `SetActiveTestItem()`; latent cmds `FTD_AssertBathroomDoorLockedSoundCount` + `FTD_SetActiveItem`.

**TDD.** RED (no guard): `count=1, expected 0` ❌ — confirms the test reproduces the bug. GREEN (guard): `count==0` ✅ (9 steps, headless). **Regression:** `HappyPath` ✅ green (131 steps) — the `UseKeyOnDoor` path is unaffected.

Files: `OutsideBathroomDoor.h/.cpp`, `TestDriverSubsystem.h/.cpp`, `E2E_LatentCommands.h`, `E2E_Level1Test.cpp`.

---

## Item 2 — Self-illuminating held/inspected items ✅

**Approach (your call, mid-run).** You asked about an outline shader; we weighed a true post-process/inverted-hull outline (more readable but gamey, cuts against the diegetic rule) vs. a rim-heavy emissive glow (diegetic — the object emits light). You picked the **rim-heavy emissive**.

**What it does.** New material `/Game/CreatedMaterials/M_ItemDarkGlow` — Unlit + Additive overlay, `Emissive = (EmissiveFloor + Fresnel*RimStrength) * GlowColor / EyeAdaptation`. The small floor (0.06) gives the item body so it's recognizable; the Fresnel rim (exp 4, strength 9) brightens the edges; the EyeAdaptation divide makes it render at constant brightness regardless of the scene's auto-exposure; warm color (1.0, 0.86, 0.6). Item-agnostic — applied as a component overlay (`SetOverlayMaterial`), no per-item base-material edits.

**Where applied.** `HeldItemComponent::ShowHeldItem` sets the overlay, `HideHeldItem` clears it (nothing held → no glow). `InspectablePickup` sets it on the pickup mesh while inspected, clears on put-back. The environment receives no added light (emissive-only) — darkness preserved.

**Assert seam.** Driver `GetHeldItemGlowActive()` (visible held mesh has a non-null overlay); latent cmd `FTD_AssertHeldItemGlow`.

**TDD.** RED (no overlay code): `glow=false` ❌. GREEN: `glow=true` ✅ (6 steps, headed). **Visual proof:** `Saved/Screenshots/WindowsEditor/E2E_DarkGlow_KeyHeld.png` — a warm, fully-legible key in a pitch-black room, environment dark. (Held-item flatscreen pose tucks it lower-right; that's the production pose, not the glow.)

**Tuning.** All glow values are baked defaults in `scripts/local/create_item_dark_glow.py` (the .uasset is a build artifact in `Content/CreatedMaterials/`). Re-run that script to retune floor/rim/exponent/color, then re-run the headed test — no C++ rebuild needed.

Files: `M_ItemDarkGlow.uasset` (new), `HeldItemComponent.h/.cpp`, `InspectablePickup.h/.cpp`, `TestDriverSubsystem.h/.cpp`, `E2E_LatentCommands.h`, `E2E_Level1Test.cpp`.

### If you'd prefer a true outline later
Noted but not built: (2) custom-depth stencil + post-process edge-detect material — crisp, lighting-independent, but reads as a gamey UI highlight; (3) inverted-hull mesh outline — hard outline in world-space, fiddly on thin meshes. Say the word and I'll swap the overlay for one of these.

---

## Verification method
- Editor closed before every `Build.bat weirdplace2Editor Win64 Development`.
- `run_e2e.ps1 -TestName <Name>`; results read from `Saved/Logs/E2ETest.log` scoped to the latest `=== E2E TEST START ===`.
- Final gate: `run_e2e.ps1 -TestName Regression -Headed -TimeoutMinutes 60` → **17/17 green, 316 steps.**

## Commits (on `overnight/2026-06-19`)
- `af1ba47e` — Item 1: guard bathroom door against re-entrant interact during key-break
- `98396938` — Item 2: self-illuminating glow overlay for held/inspected items
- `d9109eea` — Nightly report: certify full Regression suite green (17/17, 316 steps)
- `75b69c16` — Follow-up: keep the glow on the key during the door-lock insert animation (re-verified: HappyPath green, 131 steps)
- `856a0b85` — Follow-up: make the dropped broken-key pickup glow on the ground, not just inspected (re-verified: HappyPath green, 131 steps)

## Open decisions for you
- **Material recipe location.** `scripts/local/create_item_dark_glow.py` (the glow generator/tuner) is in gitignored `scripts/local/`, so the committed record is the `.uasset`. Move it to `scripts/` if you want the recipe version-controlled.
- **Outline alternative.** If the rim-glow isn't enough in-game, I can swap to a custom-depth stencil post-process outline or an inverted-hull outline (details in the Item 2 section).
- Nothing merged. Merge target in the morning is `overnight/2026-06-17`.
