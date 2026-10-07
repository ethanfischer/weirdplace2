# Nightly Report — 2026-10-06

**Home branch:** overnight/2026-09-15
**Overnight branch:** `overnight/2026-10-06`
**Scope agreed:** (1) Car ride ends with a real drive + pull-over instead of a black fade; gas-station lights must not pop. (2) Reset player walk speed to the UE first-person default.
**Result:** in progress

> Kickoff note: you went to bed in the same message that handed this off, so the
> acceptance-criteria interview below was written by me, not ratified by you.
> Treat anything marked *(assumed)* as my call.

## Agreed acceptance criteria — locked at kickoff (written before going autonomous)
### 1. Car ride: real drive + pull-over (`Weirdplace2.E2E.Level1.Regression.CarRideDrive`)
- [ ] After the ride-end trigger, BP_Car's world location changes over time and its distance to Rick's parking spot (OutsidePositionTarget + CarActorOffset) strictly decreases across ≥3 samples — proven by a new `FTD_SampleCarDrive` latent command  *(RED-defining)*
- [ ] The car comes to rest within 50 cm / 5° of the parking spot (same place `Rick::AppearOutside` would put it) — `FTD_AssertCarParked`
- [ ] While the car is moving, the camera fade amount is 0 (no black) — sampled in `FTD_SampleCarDrive`
- [ ] While the car is moving, the player's eye stays on PassengerSeatTarget (<30 cm) — sampled in `FTD_SampleCarDrive`
- [ ] Gas-station lights do not pop: at the frame they are unhidden their intensity is ≤10% of authored, and it reaches authored by the end of the ramp — `FTD_AssertGasStationLightsRamp`
- [ ] Ride end state unchanged: player at GasArrivalTarget, MOVE_Walking, can interact — existing asserts
- [ ] Screenshots: before unhide, right after unhide, mid-drive, pull-over, parked, after exit — eyeballed for no visible pop
- *(assumed)* Getting out of the car is a short dip to black (~0.4 s) rather than a hard cut; the long 3s/2s/3s fade is gone.
- *(assumed)* The car turns 90° into the pumps (parked yaw is perpendicular to the approach road), with a constant-deceleration stop.

### 2. Walk speed reset
- [ ] BP_FirstPersonCharacter CharacterMovement MaxWalkSpeed == 600 (UCharacterMovementComponent default; was 300) — asset edit, verified by reading the CDO back  *(RED-defining: was 300)*

## Done — needs your eyes before merge

## Blocked / WIP

## Skipped

## How to review this run
- `git log --oneline overnight/2026-09-15..overnight/2026-10-06`
- `git diff overnight/2026-09-15..overnight/2026-10-06`
- Screenshots: `Saved/Screenshots/Windows/E2E_CarRideDrive_*.png`
