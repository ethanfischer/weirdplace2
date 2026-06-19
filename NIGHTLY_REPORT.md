# Nightly Report — Tornado/Telephone story beats (2026-06-17)

**Branch:** `overnight/2026-06-17` (off `June17`) · **Nothing merges to `June17` until you play the build and OK a squash-merge.**

**Result:** all 6 (Foundation + 5 todos) green — 0 blocked, 0 skipped. One commit per item. Final tip certified: the full **`Regression` suite is green — 14/14 tests, 280 steps** (the 5 new regression tests + the 9 pre-existing, all passing together). The whole narrative chain works end to end: key breaks → both store TVs flip to a tornado warning → gazing at one unlocks the roadside pay-phone scene → Seneca's smoking dialogue gains a tornado-shelter tip → the pay phone plays static/voices once → a MISSING PERSON / SENECA poster hangs on the pole. **Update (you, post-run):** you authored that poster directly in `BP_TelephoneScene` with a real Seneca photo, so my placeholder runtime poster was removed — see item 3.

Order run: Foundation → 4 → 1 → 2 → 5 → 3 (clean E2E win first, then the dependency chain, asset-heavy items last).

---

## Status at a glance

| Item | Beat | Test | Result |
|------|------|------|--------|
| Foundation | central story-flag subsystem | `Regression.StoryFlags` | ✅ green |
| 4 | Seneca tornado-shelter line | `Regression.SenecaShelterLine` | ✅ green |
| 1 | TVs show tornado warning on store entry | `Regression.TornadoWarningOnStoreEntry` | ✅ green |
| 2 | Telephone scene gated on SeenTornadoWarning | `Regression.TelephoneGatedOnWarning` | ✅ green |
| 5 | Pay phone static audio | `Regression.PayPhoneStatic` | ✅ green |
| 3 | Missing-person poster of Seneca | (designer-authored in BP) | ✅ done (your poster) |

---

## Locked acceptance criteria (from kickoff)

### Foundation — `UStorySubsystem`
- Infra guard: set a flag via test hook → `IsStoryFlagSet` returns true. Flag *effects* are proven by items 1/2/4/5.

### Item 4 — Seneca shelter line (gated on `SeenTornadoWarning`)
- [ ] With `SeenTornadoWarning` set, Smoking lines contain the shelter token ("shelter"/"stall")  *(RED-defining)*
- [ ] With the flag unset, Smoking lines do NOT contain the shelter line (gating)
- [ ] Screenshot shows Seneca's shelter line on the dialogue widget

### Item 1 — TVs show tornado warning (both TVs)
- [ ] After `SetFlag(KeyBroke)` + `TriggerStoreEntry()`, both TVs report `bShowingWarning==true`  *(RED-defining)*
- [ ] Without `KeyBroke`, store entry does NOT switch the TVs
- [ ] After the warning shows, looking at a TV for the dwell sets `SeenTornadoWarning`
- [ ] Screenshot shows a TV on the warning screen

### Item 2 — Telephone scene gated (visibility only)
- [ ] With `SeenTornadoWarning` unset, `BP_TelephoneScene` root is NOT visible  *(RED-defining)*
- [ ] After `SetFlag(SeenTornadoWarning)`, root IS visible
- [ ] Screenshot confirms pole+payphone render with correct materials post-reparent

### Item 5 — Pay phone static (press E, once, gated)
- [ ] With `SeenTornadoWarning` set, triggering pickup starts the phone audio  *(RED-defining)*
- [ ] Second pickup does NOT restart it (one-shot)
- [ ] Without the flag, pickup does nothing
- [ ] Screenshot of the phone on pickup

### Item 3 — Missing-person poster (always present, screenshot-only)
- Hard assert: an actor labeled `MissingPersonPoster` exists at the pole.
- Look judged by reading `E2E_MissingPersonPoster_*` (headed). Real Seneca-head render attempted (max 5 tries), placeholder fallback otherwise.

---

## Per-item log

- ✅ **Foundation — `UStorySubsystem`** — `Regression.StoryFlags` green (8 steps, headed).
  New `UStorySubsystem : UWorldSubsystem` (`StorySubsystem.h/.cpp`) holds `EStoryFlag { KeyBroke, TornadoWarningDisplayed, SeenTornadoWarning }` in a per-world `TSet`, with `SetFlag`/`IsFlagSet`/`OnStoryFlagChanged` (broadcasts on change). `OnWorldBeginPlay` binds `TriggerBox_Inside`'s `OnActorBeginOverlap` (player-only) → `HandleStoreEntry()` (TV logic filled in item 1). `AOutsideBathroomDoor::SpawnBrokenKeyPickup` now **additively** sets `KeyBroke` (Seneca-smoking path untouched). TestDriver hooks: `SetStoryFlag`/`IsStoryFlagSet`/`TriggerStoreEntry`; latent cmds `FTD_SetStoryFlag`/`FTD_AssertStoryFlag`.
  RED→GREEN: stubbed `IsFlagSet`→false first (assert failed exactly as designed), then real `TSet::Contains` → pass.
  **Run note:** the automation editor-cmd crashes at startup under `-NullRHI` restoring the saved **Fab browser tab** (`FFabBrowser::OpenTab` → invalid SharedPtr — web browser can't init without RHI). Worked around by running E2E **`-Headed`** (real RHI lets the Fab tab init). Deterministic, not flaky; unrelated to game code. *(This is why every test below is run headed.)*

- ✅ **Item 4 — Seneca shelter line** — `Regression.SenecaShelterLine` green (14 steps, headed). Screenshot `E2E_SenecaShelterLine_Dialogue.png` shows Seneca smoking + the line *"…that twister's no joke. There's a shelter under the far stall…"* on the widget.
  Refactored the default-path line assembly into `ASeneca::BuildEffectiveDialogueLines(State, Out)` — single source of truth for `Interact_Implementation` and the test hook. It appends the shelter tip only when `State==Smoking && Story->IsFlagSet(SeenTornadoWarning)`. RED→GREEN: appended-line stubbed out first (set→contains assert failed), then enabled. Gating proven by the unset-→-omit assert in the same test. Drove the screenshot via new `ForceSenecaToSmoking` hook (jumps her into the beat) + `FTD_AdvanceDialogueUntilLineContains`.
  **Verify in-game:** the shelter line only appears if you actually watched the tornado warning on the TVs before reaching Seneca smoking outside. Placeholder wording — tweak in `Seneca.cpp` `BuildEffectiveDialogueLines`.

- ✅ **Item 1 — TVs show tornado warning on store entry** — `Regression.TornadoWarningOnStoreEntry` green (14 steps, headed). Screenshot `E2E_TornadoWarning_Screen.png` shows a TV switched to a saturated red emergency-broadcast screen. Regression gate clean: HappyPath + GazeReward + GazeRewardReset all green.
  `ACRTTV::ShowTornadoWarning()` closes the media feed and swaps the screen slot (found by the "Screen" material-name) to placeholder `M_TornadoWarning` (unlit red emissive, **divided by EyeAdaptation** so it doesn't blow out to white under the dark store's auto-exposure). `UStorySubsystem::HandleStoreEntry` switches **both** TVs + sets `TornadoWarningDisplayed` only when `KeyBroke && !TornadoWarningDisplayed`, then runs a 0.1s gaze-watch timer that sets `SeenTornadoWarning` after ~2s of looking at a warning TV. Gaze geometry extracted into shared `UGazeUtils::IsActorInPlayerGaze` (GazeRewardComponent re-pointed at it — gaze tests still green).
  RED→GREEN: TVs not switching first (HandleStoreEntry no-op) → both `bShowingWarning` asserts + flag failed; then the switch + gaze-watch landed green. CEF/libcef headed flake hit once on a re-run — retried, passed (known flake).
  **Verify in-game:** break the bathroom key, walk back into the store — both CRTs should flip to the red warning. Stare at one ~2s to "see" it (unlocks the telephone scene + Seneca's shelter line). Placeholder red screen; real broadcast feed is a later swap (`M_TornadoWarning`).

- ✅ **Items 2 + 5 — Pay-phone scene gated + static audio** — `Regression.TelephoneGatedOnWarning` (7 steps) + `Regression.PayPhoneStatic` (14 steps) green; HappyPath regression gate green after the reparent. Screenshot `E2E_Telephone_Revealed.png` shows the revealed pole rendering with its material.
  New C++ `APayPhone : AActor, IInteractable`; **`BP_TelephoneScene` reparented onto it** (asset query confirms `is_child_of_PayPhone=True` and both override materials intact — `MI_Telephone_Pole_01a`, `MI_Pay_Phone_NN_01a`). `BeginPlay` hides the root (`SetVisibility(false)`) and subscribes to `OnStoryFlagChanged`; reveals on `SeenTornadoWarning` (or immediately if already set). `Interact` plays a placeholder static (`WindInside`) + voices (`LowVoiceSoundCue`) bed **once**, gated on `SeenTornadoWarning`; `CanInteract` returns `SeenTornadoWarning && !bPlayedOnce`. Driven in tests via `TriggerPayPhonePickup`/`CanPayPhoneInteract`/`IsPayPhoneAudioPlaying` (not raw key — 5.7 input gotcha).
  RED→GREEN: pre-reparent the scene is always visible + there's no `APayPhone` (both asserts failed); after reparent both green.
  **Verify in-game:** the pay-phone + pole only appear after you've seen the tornado warning. Walk up and press E once — static + faint voices play (one-shot). **Audition** the placeholder sounds and swap real static/voice on the `APayPhone` BP (`StaticSound`/`VoiceSound`).

- ✅ **Item 3 — "Missing person" poster of Seneca** — **you authored this directly in `BP_TelephoneScene`** (a real `PosterSheet` plane with Seneca's photo, "MISSING", and "Last seen May 4, 1993 … call 217-312-1573", parented under the pole). My overnight placeholder (`AMissingPersonPoster` runtime actor + `M_PosterPaper` + the `Diagnostic.MissingPersonPoster` test + TestDriver/latent hooks) is **removed** at your request — your version is better and the head-likeness is real. Net: a clean deletion commit.
  **Note on gating:** because your poster is a child of the telephone scene's root, `APayPhone::BeginPlay`'s `SetVisibility(false)` hides it along with the pole until `SeenTornadoWarning` — i.e. the poster now **reveals with the scene** rather than being always-present. That resolves the old "poster floats before the pole" wrinkle. If you ever want it visible *before* the warning, move it out from under the gated root.

---

## Known wrinkles to flag

- **Item 3 poster (your BP version)** is parented under the telephone scene root, so it's hidden until `SeenTornadoWarning` and reveals with the pole. Move it out from under the gated root if you want it visible earlier.
- **Placeholder art/audio** swapped in later: TV warning = flat red `M_TornadoWarning`; pay-phone = `WindInside` (static) + `LowVoiceSoundCue` (voices); Seneca's shelter line is drafted prose. (The poster art is now real — your work.)
- **Headed E2E + Fab/CEF flake:** the automation editor restores the saved **Fab browser tab**, which asserts (`FFabBrowser::OpenTab` / `CEFWebBrowserWindowRHIHelper`) — fatal under `-NullRHI`, intermittent even headed. So every test here was run **`-Headed`**, and one run was retried past a CEF crash. Unrelated to game code; closing the Fab tab in your editor before an automation run would remove it entirely. I did **not** touch your global editor layout config (left it as-is).
- **`TriggerBox_Inside` binding** uses the editor label (works in PIE) + a `StoreEntryTrigger` actor tag (not yet placed). For a packaged build, add that tag to the inside trigger; E2E drives `HandleStoreEntry` directly so it isn't on the test's critical path.

## Verify-in-game steps

Play from a fresh start and walk the chain:
1. Do the bathroom-key beat until the key **snaps** in the lock (drops the broken-key pickup). → sets `KeyBroke`.
2. Walk **back into the store** → both CRT TVs flip to a **red tornado-warning** screen. Stand and look at one for ~2s.
3. Head out to the **roadside telephone pole** → the pole + pay phone are now **visible** (they were hidden before you saw the warning). A **MISSING PERSON / SENECA** poster is stapled to the pole.
4. Press **E** on the pay phone → **static + faint voices** play (once only).
5. Talk to **Seneca smoking outside** → her lines now include the **tornado-shelter** tip ("…shelter under the far stall…").

(Reminder: the screens/audio/poster are placeholders — judge the *logic/flow*, not the final art.)

## How to review this run
- `git log --oneline June17..overnight/2026-06-17` — the per-item commits (5).
- `git diff June17..overnight/2026-06-17` — the full change.
- Screenshots: `Saved/Screenshots/WindowsEditor/E2E_{SenecaShelterLine,TornadoWarning,Telephone,PayPhoneStatic,MissingPersonPoster}_*.png`.

---
**Not merged — waiting on your verification.** Green tests + screenshots got each item this far, but nothing lands on `June17` until you play it. Tell me how you want the merge handled (squash / cherry-pick / plain / leave / discard).

---

# Addendum — Tornado warning "storm intensifies" (2026-06-18)

**Branch:** `overnight/2026-06-17` (continued). Extends **item 1**: the tornado-warning beat now *feels* like the storm closing in. Three connected additions, all firing at the same existing `EStoryFlag::TornadoWarningDisplayed` moment.

**Result:** new **`Regression.TornadoWarningStormBeat` green (17 steps, headed)**. Regression gate clean: **`TornadoWarningOnStoreEntry` (14)** + **`HappyPath` (131)** both green. Screenshot `E2E_TornadoStormBeat_Screen.png` shows a TV glowing storm-red in a darkened store (the red fallback — no designer texture assigned yet).

The E2E self-configures a controller at runtime (PIE spawn is safe; it's *headless-editor* spawn of C++ classes that crashes in 5.7), so the whole **mechanism is proven**. The in-game beat still needs a one-time level wiring — see the **MANUAL SETUP** box below.

## What landed (code — verified by E2E)
1. **The store's TV ambient beds cease** — `Ambient_TV` + `Ambient_TV2` stop the moment the warning shows.
2. **The TVs blare a looping tornado-alert siren** — `/Game/Sounds/tornadoalert` (CC0, *EAS Alarm* by mpaol2023 — credited) loops diegetically from each TV over the warning screen. The SoundWave is set `bLooping=true` (a `UAudioComponent` won't loop a one-shot wave on its own). The siren has an attenuation override so it's loudest at the TVs and falls off with distance (point source, not a level-wide blast).
3. **The gas-station lights dim and stay dimmed** — each referenced light's intensity × an editor-set multiplier, applied once.

### New / changed code
- **`ACRTTV`** (`CRTTV.h/.cpp`) — new `WarningScreenTexture` (designer art slot) + `WarningSound` (defaults to `tornadoalert`) + a runtime `WarningAudio` component (spatialized, attenuated, built in `BeginPlay`). `ShowTornadoWarning` now builds a **MID** from `M_TornadoWarning` and plays the looping siren; `IsWarningAudioPlaying()` added for tests.
- **`M_TornadoWarning`** regenerated (`scripts/local/gen_tornado_warning_material.py`) with a `ScreenTex` texture param + a `UseScreenTex` scalar switch: the material **lerps** the existing storm-red ↔ the texture (both still `/ EyeAdaptation` for stable brightness). Stays red until C++ binds `WarningScreenTexture` (then shows the art untinted) — no orphan placeholder texture asset needed.
- **`AStormBeatController`** (new `StormBeatController.h/.cpp`) — a standalone placed actor: `LightsToDim` (array), `DimMultiplier` (0–1), `AmbientSoundsToSilence` (array). Subscribes to `OnStoryFlagChanged`; `ApplyStorm()` dims + silences once on `TornadoWarningDisplayed` (guards re-entry; subscribe/teardown mirrors `APayPhone`).
- **`tornadoalert`** SoundWave set looping (`scripts/local/set_tornadoalert_looping.py`).
- Test infra: 4 TestDriver hooks (`IsTvWarningAudioPlaying`, `IsAmbientSoundPlaying`, `GetActorMaxLightIntensity`, `SpawnAndConfigureStormBeat`), 5 latent commands, `Regression.TornadoWarningStormBeat`. `credits.md` updated.

## ⚠️ MANUAL DESIGNER SETUP — required for the in-game beat
The mechanism is E2E-proven, but the real beat needs you to wire the level **once**:

1. **Place one `AStormBeatController`** in `FirstPersonMap` (anywhere — it has no mesh).
2. On that controller's **Details panel (level instance)**:
   - **`LightsToDim`** — drag in the exact gas-station light actors. Confirmed actors with a `ULightComponent`: `SpotLight3`, `SpotLight10`–`SpotLight17` (100 lm canopy spots), `OasisBigLight` (160 cd). ⚠️ The `gastationbarlights` / `outsidegastationlights` from the todo that are emissive **meshes** (no light component) **can't be dimmed this way** — only actors with a real light component respond. If you need those dimmed too, tell me and I'll add an emissive-material path.
   - **`DimMultiplier`** — your storm-dim factor, 0–1 (the test used `0.3`).
   - **`AmbientSoundsToSilence`** — drag in `Ambient_TV` and `Ambient_TV2`.
3. **Assign the screen texture** — on **`BP_TV` class defaults**, set **`WarningScreenTexture`** to your "TORNADO WARNING" image (both TVs share the one assignment). Until you do, the screen shows the storm-red fallback (see screenshot).

Then play it: break the bathroom key → re-enter the store → both CRTs blare the looping alarm over the warning texture, `Ambient_TV`/2 cut out, and your referenced lights drop to the multiplier and stay there.

**Files:** `Source/weirdplace2/{CRTTV,StormBeatController,TestDriverSubsystem,E2E_LatentCommands,E2E_Level1Test}.*`, `scripts/local/{gen_tornado_warning_material,set_tornadoalert_looping}.py`, `credits.md`. Screenshot: `Saved/Screenshots/WindowsEditor/E2E_TornadoStormBeat_Screen.png`.
