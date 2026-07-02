# Nightly Report — 2026-07-01

**Home branch:** `July1`
**Overnight branch:** `overnight/2026-07-01`
**Scope agreed:** posters → inspection blur → seneca text → clock blur → first-play investigation
**Result:** (in progress)

> The previous 2026-06-19 nightly report is preserved in git history; this file was
> overwritten for tonight's run.

## Agreed acceptance criteria — locked at kickoff (written before going autonomous)

### 1. Collected movies appear as posters (telephone pole + bathroom)
Ratified: source = movies the player **collected**; update **live on collection**; the two
locations show **different** movies; **new poster meshes** placed by Claude; posters
**hidden until collection**. Mapping (Claude's call): first collected → pole, second → bathroom.
- [ ] Before any collection, both posters are not visible — poster visibility assert
- [ ] After collecting movie A, the pole poster becomes visible with A's cover texture — new `FTD_` assert *(RED-defining)*
- [ ] After collecting movie B, the bathroom poster shows B; pole still shows A; textures differ
- [ ] Headed screenshots of both posters post-collection show real cover art (not blank/checkerboard)
- [ ] `HappyPath` still green (touches collect path)

### 2. Blur background during item inspection
Ratified: **flatscreen only** — gate on `GEngine->XRSystem->IsHeadTrackingAllowed()`.
- [ ] During MovieBox inspection (`Interacting`), blur post-process active on player camera (blendable weight > 0) *(RED-defining)*
- [ ] Weight is 0 before inspection and after exit
- [ ] Headed screenshot mid-inspection: background blurred, inspected item sharp
- [ ] VR gate present in code (verified by review; E2E runs flatscreen)
- [ ] `HappyPath` still green (touches inspection flow)

### 3. Seneca dialogue text legible against bright light
Ratified: **dark backing panel** (subtle translucent plate behind the text).
- [ ] During Seneca dialogue, backing panel visible behind the text *(RED-defining)*
- [ ] Panel hidden when no dialogue active
- [ ] Headed screenshot framing text against the bright light — text legible (Claude judges the frame)

### 4. Clock blurred out
Ratified: **fully illegible** — reads as a clock, time unreadable. Screenshot-only item
(no hollow runtime test).
- [ ] Clock face blurred so hands/numbers are unreadable in a headed close-up screenshot
- [ ] Object still recognizably a clock

### 5. Fog wall + bladder indicator invisible on first play after editor launch
Ratified: symptom = **invisible/missing** on first PIE, fine on second; **warmup at load
is an acceptable fix** if root cause is first-use shader/PSO/Niagara compilation.
- [ ] Root cause identified with evidence (logs/trace, not vibes)
- [ ] If confirmed: load-time warmup implemented so first play renders both effects
- [ ] Best-effort repro documented; if un-reproducible in the harness, evidence-backed writeup + fix, user verifies with a fresh editor launch in the morning

## Done — needs your eyes before merge
- ✅ **Collected movies appear as posters** — `Weirdplace2.E2E.Level1.Regression.MoviePosters` green (24 steps).
  Criteria: ✅ hidden before collection · ✅ pole shows first collected cover (RED-defining; was a genuine red first) · ✅ bathroom shows second, pole unchanged, textures differ · ✅ screenshots show real cover art · ✅ HappyPath green (124 steps).
  Screenshots: `E2E_Poster_01_Pole.png` (cover framed on the pole flyer sheet, readable at night), `E2E_Poster_02_Bathroom.png` (portrait cover above the employee-bathroom sinks).
  **Verify in-game:** gaze the tornado TV to reveal the phone scene, collect one movie → the pole's flyer sheet by the payphone shows its cover; collect a second → the employee-bathroom west wall shows that one.
  **How it works:** new `UMoviePosterSubsystem` — records distinct movies in collection order, drives two tagged surfaces (`MoviePoster0` = the phone scene's existing `PosterSheet` component, `MoviePoster1` = a new plane placed on the bathroom wall) with `M_VHSCoverFront` MIDs over `/Game/VHSCovers/<ItemID>` (the MI_VHSCover_* materials are full box wraps — wrong on flat surfaces; same crop pipeline as inventory thumbnails).
  **Decisions made solo:**
  - *Discovery:* the pole already had a designed "missing poster" (white flyer + `missingposter_Mat` photo region) — the interview's "nothing exists at either spot" premise was wrong for the pole. Per the locked criteria the sheet is now **hidden until a movie is collected**, so the missing-poster art never shows. If you'd rather keep it as the pre-collection state (flyer gets pasted over by your first pick — arguably better), it's a two-line change in `ApplyPosterStates`; say the word.
  - Pole poster also gates on the phone scene's `SeenTornadoWarning` reveal (no poster floating next to an invisible pole).
  - Posters persist after giving tapes to Seneca (the world remembers your picks).
  - Mapping: first collected → pole, second → bathroom (employee bathroom, west wall).

- ✅ **Blur background during item inspection** — `Weirdplace2.E2E.Level1.Regression.InspectionBlur` green (11 steps).
  Criteria: ✅ DoF blur active on the player camera during inspection (RED-defining; genuine red first — "DoF overrides not active") · ✅ inactive before and fully cleared after · ✅ screenshot mid-inspection shows sharp held box, blurred shelf behind · ✅ VR gate in code (`XRSystem->IsHeadTrackingAllowed()`, component self-disables in VR) · ✅ HappyPath green (124 steps).
  Screenshots: `E2E_InspectBlur_01_During.png` (inspected tape sharp, shelf soft), `E2E_InspectBlur_02_After.png` (world crisp again).
  **Verify in-game:** inspect any shelf movie — the store behind it melts into bokeh; put it back or collect and focus returns.
  **How it works:** new `UInspectionBlurComponent` on `AFirstPersonCharacter` — ramps cinematic DoF overrides (focal 45cm, f/22→f/1.0 over ~0.33s) on the first-person camera while activity state is `Interacting`. Note: asserts read the camera's DoF override state directly (the observable form of the ratified "blendable weight > 0") so the test compiles without any feature dependency.

## Blocked / WIP
(nothing yet)

## Skipped
(nothing yet)

## How to review this run
- `git log --oneline July1..overnight/2026-07-01` — the per-item commits
- `git diff July1..overnight/2026-07-01` — the full change
- Screenshots: `Saved/Screenshots/Windows/E2E_*.png`
- Parked attempts: `git branch --list 'wip/*'`

---
**Not merged — waiting on your verification.** Nothing lands on `July1` until you play it
and confirm.
