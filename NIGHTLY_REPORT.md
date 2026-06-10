# Nightly Report — 2026-06-09

**Home branch:** `May31`
**Overnight branch:** `overnight/2026-06-09`
**Scope agreed (plan-mode approved):** 1) gas-station light gaze → cash (ONE light, rising hum; TV sub-item deferred by your call) · 2) "Rick:" prefix loader-parse fix · 3) movie put-back prompt + text faces player · 4) blank VHS held pose
**Result:** 4 done · 0 blocked · 0 skipped (+1 bonus: pre-existing HappyPath flake diagnosed and fixed). Final tip: HappyPath green ×3 + all four item tests green.

## Done — needs your eyes before merge
- ✅ **Gas-station light gaze reward → cash** — `Weirdplace2.E2E.Level1.GazeReward` green (13 steps).
  Screenshots: `E2E_GazeReward_01_StaringAtLight.png` (camera locked on the glowing canopy bar light at night), `E2E_GazeReward_02_CashGranted.png` (same view + the spinning cash notification mesh in front of the camera).
  **Verify in-game:** the rigged bar is the one **just outside the store door** (level actor `gasstationbarlight`, X≈5000 — not the ones over the pumps). Stare at **any point along the bar** for 30 uninterrupted seconds — the hum fading in is your confirmation you're on the right bar; at 30s you get cash (standard pickup popup + inventory slot). Look away mid-stare → hum cuts out, timer resets. Other bars do nothing. **Audition the hum** — it's a generated 120 Hz electric-hum loop at `/Game/Sounds/gazehum`; swap `HumSoundPath` in `DefaultGame.ini` for different vibes.
  *(Morning fix after your report: the original check was a 10° cone around the bar's center POINT — fine for the test driver, useless for a human staring at a 30m fixture. Now the gaze counts when your camera ray crosses the bar's bounding box anywhere, with a proper occlusion trace.)*
  Notes / decisions made solo:
  - Editor-time placement via Python **crashes UE 5.7 headless** (`EXCEPTION_INT_DIVIDE_BY_ZERO` in the actor-factory path), so the rig is: a `GazeRewardTarget` tag on the light actor + `UGazeRewardSubsystem` spawns `AGazeRewardActor` at the light's bounds center at runtime, configured from `DefaultGame.ini` (item/sound/duration). Saved to project memory.
  - Hum is non-spatialized; volume is purely stare-progress (5% floor → 100% at 30s).
  - Gaze = 10° cone + line-of-sight trace (a hit in the last 10% of the ray counts as the fixture itself).
  - TV/movie sub-item deferred per your kickoff answer; todo sub-line left open.

- ✅ **"Rick:" prefix out of dialogue body** — `Weirdplace2.E2E.Level1.RickDialoguePrefix` green (8 steps).
  Screenshot: `E2E_RickPrefix_DialogueShown.png` (Rick by his car, line reads "I'll meet you inside once I'm done here" — no prefix).
  **Verify in-game:** at the gas station start, walk up to Rick outside and interact — the bubble should show the line without `Rick:`.
  Notes / decisions made solo:
  - Root cause: `ARick::LoadOutsideDialogue`'s idle branch loaded lines raw while every sibling loader parses `Speaker:` — fixed the idle branch to match (loader-parse, your ratified choice).
  - The displayed-dialogue assert reads the live `UUI_Dialogue` widget (new tiny getters + driver method) — reusable for any future dialogue test.
  - First red failed for the wrong reason: `FTD_TeleportNearRick`'s spot doesn't put Rick's interactable geometry on the interact ray; switched to the HappyPath's `RickApproach` waypoint. Other NPC-prefix files (Hudson/Seneca) weren't touched — this todo named Rick's idle line only.

- ✅ **Movie put-back prompt + always faces player** — `Weirdplace2.E2E.Level1.MoviePutBackPrompt` green (13 steps).
  Screenshots: `E2E_PutBack_01_PromptShown.png` / `E2E_PutBack_02_AfterRotate.png` — "Q / B  put back" floats below the inspected box and holds its facing while the box spins.
  **Verify in-game:** inspect any shelf movie — white text under the box should read `Q / B  put back`, stay readable as you rotate the box, and pressing Q (or gamepad B) should return the movie to the shelf.
  Notes / decisions made solo:
  - Prompt is a runtime-created `UTextRenderComponent` on every MovieBox (no Blueprint edit), text composed live from the `Exit Interaction` mappings — rebind the key and the prompt follows.
  - First green's screenshot showed the text occluded by the box itself; pulled it 18cm toward the camera. The component-flag assert can't catch occlusion — the screenshot did.
  - Diegetic 3D text per the VR rule; world size 4cm.

- ✅ **Blank VHS held pose matches other movies** — `Weirdplace2.E2E.Level1.BlankVHSHeldPose` green (24 steps).
  Screenshots: `E2E_VHSPose_01_MovieHeld.png` / `E2E_VHSPose_02_BlankHeld.png` — movie and blank ride the hand slot with the same tilt, size, and spot.
  **Verify in-game:** collect a movie and the blank tape, hold each from the inventory — the blank should sit in your hand like any movie (it used to be a giant box lying sideways, half off-screen).
  Notes / decisions made solo:
  - Three fixes: capture **world** scale at collect (not relative — the Memphis mesh gets its size from ancestor scaling, so the held blank was giant), a per-class `HeldPoseCorrection` rotation on MovieBox (BP_BlankVHS CDO = 180° yaw, solved from logged camera-space axis mappings), and `HeldItemComponent` now holds meshes by **bounds center** instead of pivot (the Memphis pivot is way off-mesh; centered-pivot meshes unaffected).
  - The pose assert compares camera-space box axes + size + center between held movie and held blank — mesh-authoring agnostic, reusable.

- ✅ **Bonus: HappyPath de-flaked** (not a todo item — surfaced by item 4's regression gate).
  `Weirdplace2.E2E.Level1.HappyPath` green ×3 consecutive (different random tape slots), plus all four item tests green on the final tip.
  What was wrong: the blank-tape collect has always been **slot-dependent** — `ActivateChosenTape` swaps a *random* top-shelf box each run, and the test aimed at the actor's render-bounds center, which the Tape child mesh skews off the Memphis envelope's collision. On bad slots the interact ray slipped past the box (or hit a neighbor) and the back half of HappyPath cascaded. Base `May31` passed only because it rolled a friendly slot.
  Fixes: (a) `FTD_TeleportNearBlankTape` probes both sides with the interact-trace object types and aims at the **verified-hittable surface point**; (b) restored `ChosenForwardOffset` 0→10cm on the `BP_Spawner1` instance (the C++ default — the chosen tape now stands proud of the row, which also telegraphs it visually; **eyeball this on the shelf in-game**); (c) the new put-back prompt contributes no phantom bounds (`bUseAttachParentBound`).

## Blocked / WIP
<!-- parked attempts land here -->

## Skipped
<!-- items that turned out not autonomously verifiable -->

## How to review this run
- `git log --oneline May31..overnight/2026-06-09` — the per-item commits
- `git diff May31..overnight/2026-06-09` — the full change
- Screenshots: `Saved/Screenshots/WindowsEditor/E2E_*.png`
- Parked attempts: `git branch --list 'wip/*'`

→ **Next:** the deferred "watching the movie rewards you too" sub-item is a one-evening follow-up — the gaze mechanic is built; it's one more `GazeRewardTarget`-style rig pointed at the TV (decide reward + whether it should require the movie actually playing).

---
**Not merged — waiting on your verification.** Green tests and screenshots I checked got each item this far, but nothing lands on `May31` until *you* play it and confirm. Walk the **Verify in-game** steps above, then tell me how you want the merge handled (squash / cherry-pick / plain / leave / discard).
