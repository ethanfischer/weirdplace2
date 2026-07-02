# Nightly Report — 2026-07-01

**Home branch:** `July1`
**Overnight branch:** `overnight/2026-07-01`
**Scope agreed:** posters → inspection blur → seneca text → clock blur → first-play investigation
**Result:** 4 done · 1 investigated + partial fix (needs your fresh-launch verify) · 0 skipped
**Final tip certified:** full `Regression` suite green — **23/23 tests, 429 steps** (3 new regression tests + 20 pre-existing, all passing together).

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

- ✅ **Seneca dialogue text legible against bright light** — `Weirdplace2.E2E.Level1.Regression.SenecaTextBacking` green (11 steps).
  Criteria: ✅ backing panel visible behind the text during dialogue (RED-defining; genuine red — "no component DialogueBackingPanel") · ✅ hidden when no dialogue · ✅ screenshot shows the plate dimming the ceiling lights behind the text, text crisp · ✅ HappyPath green (124 steps).
  Screenshots: `E2E_SenecaText_01_Dialogue.png` (text on the dark plate, lights dimmed behind it), `E2E_SenecaText_02_Closed.png` (plate gone, same lights full-bright — proves the plate is doing the work).
  **Verify in-game:** talk to Seneca and put a ceiling light behind him — the text now sits on a translucent dark plate.
  **How it works:** `ASeneca` floats an engine plane with new unlit-translucent `M_DialogueBacking` (black, opacity 0.55, generator script `scripts/local/create_dialogue_backing_material.py`) 1cm behind the dialogue `WidgetComponent`, inheriting its camera billboard; visibility syncs to the widget's Slate open state each tick.
  Note: Rick and Hudson share the same dialogue-widget pattern and the same illegibility risk — same fix applies if you want it there too (kept to Seneca per the locked scope).

- ✅ **Clock blurred out (fully illegible)** — screenshot-only item, per the locked criteria (no runtime logic to assert; verified via `Diagnostic.ClockClose` close-up, which stays as an on-demand diagnostic, not a Regression gate).
  Criteria: ✅ hands/numbers unreadable in a deliberate close-up · ✅ still recognizably a wall clock.
  Screenshots: `E2E_ClockClose.png` (final: smooth unreadable face in the rim) · `E2E_ClockHunt_Store_7.png` (where it lives).
  **Verify in-game:** the clock is `SM_Wall_Decor_Set_NN_02c` on the store's south wall at ~(3519, −569, 240) — walk up; you'll see a clock whose face is a smudge.
  **How it works:** the clock is a Fab "Suburban Household VOL12" decor prop; its numerals AND hands are painted into the pack's shared atlas textures. I replaced `TX_Wall_Decor_Set_NN_02a_ALB` and `_NRM` **in place** with 32px-downscale blurred versions (originals recoverable from git history; only this one prop in the level uses the atlas). The pack's `MI` has a trailing-space `'Albedo '` param that UE's material-instance API silently drops on update — hence the texture-swap approach instead of a material variant.
  Note: it took three blur iterations (albedo → +normal → strength) because hands ghosted through the normal map; the RMA was left original after a full-blur pass made the face too dark.

- 🟡 **First-play fog wall + bladder indicator** — **UPDATED after live morning debugging with you (2026-07-02)**, which corrected the overnight hypotheses:
  - **Fog wall (`ExponentialHeightFog`, the black horizon band): FIXED — `r.PSOPrecache.ProxyCreationWhenPSOReady=0` in `DefaultEngine.ini`.** Root cause: 5.7's delayed-proxy-creation loses its PSO-ready callback for the fog on a session's first world, so the fog render proxy never materializes — values all read healthy, pixels never change. Proven with an autonomous screenshot loop (fresh GUI editor sessions driven by remote-exec, `Diagnostic.FirstPlayEffects`, horizon-strip luminance metric): broken ≈ 4.8, healthy ≈ 3.3; pre-fix play 1 measured 4.76 (broken), post-fix play 1 measured 3.62 and play 2 3.36 (both healthy). PSO precaching itself (`Resources=1` + the bundled pipeline cache) stays on, so the hitch mitigation the delay belonged to is preserved — but if look-around hitches ever return, this cvar is the knob that changed.
  - ⚠️ **Landmine documented (pre-existing, not fixed):** recreating the fog's *render state mid-play* (visibility off/on across frames, `MarkRenderStateDirty`) can still kill fog for the rest of the play — deterministic pre-fix, timing-dependent post-fix. Nothing in game code does this today; if a future storm/weather beat drives the fog component's setters mid-play and the fog wall dies, this is why. (The reverted `bfe47a34` guard was this exact trap.)
  - **Bladder vignette:** first-ever render draws a corrupt border while the PP material compiles (you saw it live; clean from the 2nd render on). The overnight 0.002 warmup was below the renderer's ~1/255 weight-culling threshold — now 0.05 for the first second (still imperceptible, actually compiles).
  - HappyPath green after both fixes. **Verified by fresh-launch play:** _(pending your check)_.
  - Original overnight investigation below for the paper trail:
  **Your question — "is there some kinda shader/material compilation that only happens the first time I hit play?" — yes, almost certainly.** The chain of evidence:
  - `DefaultEngine.ini` runs 5.7's PSO precache with **proxy-creation-delayed-until-PSO-ready** (the committed perf-hitch fix) — on a **cold in-memory PSO cache** (every fresh editor launch), translucent effects skip rendering until their pipeline compiles. Second play, same session = warm cache = fine. Exactly your symptom shape.
  - The **bladder vignette had a concrete instance of this**: its post-process blendable is registered at weight 0, which is culled from rendering — so its shaders/PSO compiled at the **first real pulse**, i.e. precisely when you're supposed to see it. **Fixed:** `BladderUrgencyComponent::BeginPlay` now holds the blendable at an imperceptible 0.002 weight for the first second of play, so compilation happens at load instead. (`HappyPath` green after the change.)
  - **Repro attempt (honest result):** a fresh `UnrealEditor-Cmd` IS a cold-PSO first play, and `Diagnostic.FirstPlayEffects` screenshots the oasis waterfall steam + a forced vignette pulse at t≈3s — both rendered fine (`E2E_FirstPlay_1_*.png`). My environment resolves shaders from a warm DDC in milliseconds; your first-play window is long enough to notice, mine isn't. So the mechanism is confirmed-plausible, not confirmed-reproduced.
  - **Open question — which actor is "the fog wall"?** Nothing in the level is named fog/wall-like; my best candidate is `Blueprint_Effect_Steam` (the big Cascade steam plume at the oasis waterfall, legacy particle → likely outside PSO precache coverage). If the thing you mean is something else, tell me and the same warmup treatment applies.
  **Verify in-game (fresh launch):** close the editor fully, reopen, hit play once. Watch (a) the first bladder pulse (~60s in, or earlier in the car ride) and (b) the fog wall wherever you see it. If the pulse now shows but the fog wall still doesn't, the fog wall needs the same warm-at-load treatment — one sentence from you naming the actor and I'll wire it.

## Blocked / WIP
(nothing yet)

## Skipped
(nothing yet)

→ **Next:** the highest-value pickup is closing the first-play loop — do the fresh-launch check above and tell me which actor "the fog wall" actually is; everything else on the MVP Claude-Friendly list is done.

## How to review this run
- `git log --oneline July1..overnight/2026-07-01` — the per-item commits
- `git diff July1..overnight/2026-07-01` — the full change
- Screenshots: `Saved/Screenshots/Windows/E2E_*.png`
- Parked attempts: `git branch --list 'wip/*'`

---
**Not merged — waiting on your verification.** Nothing lands on `July1` until you play it
and confirm.
