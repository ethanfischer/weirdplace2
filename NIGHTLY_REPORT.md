# Nightly Report — 2026-10-06

**Home branch:** overnight/2026-09-15
**Overnight branch:** `overnight/2026-10-06`
**Scope agreed:** (1) Car ride ends with a real drive + pull-over instead of a black fade; gas-station lights must not pop. (2) Reset player walk speed to the UE first-person default.
**Result:** 2 done · 0 blocked · 0 skipped · HappyPath regression gate green

> Kickoff note: you went to bed in the same message that handed this off, so the
> acceptance-criteria interview below was written by me, not ratified by you.
> Treat anything marked *(assumed)* as my call.

## Agreed acceptance criteria — locked at kickoff (written before going autonomous)
### 1. Car ride: real drive + pull-over (`Weirdplace2.E2E.Level1.Regression.CarRideDrive`)
- [x] After the ride-end trigger, BP_Car's world location changes over time and its distance to Rick's parking spot (OutsidePositionTarget + CarActorOffset) strictly decreases across ≥3 samples — proven by a new `FTD_SampleCarDrive` latent command  *(RED-defining)*
- [x] The car comes to rest within 50 cm / 5° of the parking spot (same place `Rick::AppearOutside` would put it) — `FTD_AssertCarParked`
- [x] While the car is moving, the camera fade amount is 0 (no black) — sampled in `FTD_SampleCarDrive`
- [x] While the car is moving, the player's eye stays on PassengerSeatTarget (<30 cm) — sampled in `FTD_SampleCarDrive`
- [x] Gas-station lights do not pop: at the frame they are unhidden their intensity is ≤10% of authored, and it reaches authored by the end of the ramp — `FTD_AssertGasStationLightsRamp`
- [x] Ride end state unchanged: player at GasArrivalTarget, MOVE_Walking, can interact — existing asserts
- [x] Screenshots: before unhide, right after unhide, mid-drive, pull-over, parked, after exit — eyeballed for no visible pop
- *(assumed)* Getting out of the car is a short dip to black (~0.4 s) rather than a hard cut; the long 3s/2s/3s fade is gone.
- *(assumed)* The car turns 90° into the pumps (parked yaw is perpendicular to the approach road), with a constant-deceleration stop.

### 2. Walk speed reset
- [x] BP_FirstPersonCharacter CharacterMovement MaxWalkSpeed == 600 (UCharacterMovementComponent default; was 300) — asset edit, verified by reading the CDO back  *(RED-defining: was 300)*

## Done — needs your eyes before merge
- ✅ **Car ride: real drive + pull-over** — `Weirdplace2.E2E.Level1.Regression.CarRideDrive` green, `CarRideScenery` still green.
  Criteria: ✅ car moves & distance to parking decreases (42852 → 1126 → 0 cm over 11.6 s at test speed) · ✅ parked within 50 cm/5° of Rick's spot (exact) · ✅ fade 0 while moving · ✅ player eye locked to seat · ✅ 12 movable station lights at 0% on first visible frame, 100% after ramp · ✅ player at GasArrivalTarget, walking, can interact.
  Screenshots: `Saved/Screenshots/WindowsEditor/E2E_CarRideDrive_0{1..5}_*.png` — 01 right after the unhide shows no glow at all (station is 430 m out in the fog); 02/03 the canopy resolves out of the fog and grows; 04 parked beside the pumps from inside the car; 05 outside on foot.
  **Verify in-game:** play from the car-ride start, finish Rick's dialogue, and watch: the car should keep rolling, the station should fade up out of the fog ahead, the car brakes and turns in beside the pumps, short blink, you're standing outside. Watch for (a) any light pop when the station appears, (b) silhouette bushes/poles ending up in the forecourt, (c) Rick/headlight beams not riding along.
  Decisions made solo (you were asleep): getting out of the car is a 0.4 s dip to black + 0.8 s fade-in rather than a hard cut (tunables `weird.CarRide.ExitDipOut/ExitDipIn`, component `ExitDipOutDuration/ExitDipInDuration`). The car approaches straight and takes a 1500 cm-radius quarter-turn into the spot (`DriveTurnRadius`), braking over the last `DriveBrakingDistance`+turn. At the real 2000 cm/s ride speed the drive is ~25 s — tune `RideSpeed` or `PostDialogueRideTime` if that's too long. Emissive sign meshes (not lights) are unhidden at 430 m in fog; I saw no pop in the frames but a baked-emissive pop is theoretically possible — check (a) above. The old `RideEndFadeOut/BlackHold/FadeIn` properties and their cvars are gone.
  Level changes: `S_EV_SimpleLightBeam_03/_4` got the actor tag `CarRideAttached` so they ride along (map saved). At ride start the staged car is re-aimed ~1° so its lane points at the turn-in point.
- ✅ **Walk speed reset** — `BP_FirstPersonCharacter` CharacterMovement MaxWalkSpeed 300 → 600 (UCharacterMovementComponent default). Read back from the CDO after save. Crouch speed left at 300 (already the default). No E2E; asset-only.
  **Verify in-game:** walking should feel like a stock first-person template.

## Blocked / WIP

## Skipped

## How to review this run
- `git log --oneline overnight/2026-09-15..overnight/2026-10-06`
- `git diff overnight/2026-09-15..overnight/2026-10-06`
- Screenshots: `Saved/Screenshots/Windows/E2E_CarRideDrive_*.png`
- Parked attempts: none (`git branch --list 'wip/*'` is empty)

→ **Next:** play the ride at real speed (2000 cm/s ≈ 25 s drive) and decide if `RideSpeed`/`PostDialogueRideTime`/`DriveBrakingDistance` need dialing; everything is a live cvar/property.

---
**Not merged — waiting on your verification.** Green tests and my screenshots got
each item this far, but nothing lands on `overnight/2026-09-15` until *you* play it and
confirm. Walk the **Verify in-game** steps above, then tell me to squash-merge
`overnight/2026-10-06` into `overnight/2026-09-15` (or straight into `master`).
