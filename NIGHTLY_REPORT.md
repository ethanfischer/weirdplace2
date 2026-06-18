# Nightly Report — Tornado/Telephone story beats (2026-06-17)

**Branch:** `overnight/2026-06-17` (off `June17`) · **Nothing merges to `June17` until you play the build and OK a squash-merge.**

Order: Foundation → 4 → 1 → 2 → 5 → 3 (clean E2E win first, then the dependency chain, asset-heavy items last).

---

## Status at a glance

| Item | Beat | Test | Result |
|------|------|------|--------|
| Foundation | central story-flag subsystem | `Regression.StoryFlags` | ✅ green |
| 4 | Seneca tornado-shelter line | `Regression.SenecaShelterLine` | ✅ green |
| 1 | TVs show tornado warning on store entry | `Regression.TornadoWarningOnStoreEntry` | 🔨 in progress |
| 2 | Telephone scene gated on SeenTornadoWarning | `Regression.TelephoneGatedOnWarning` | ⏳ |
| 5 | Pay phone static audio | `Regression.PayPhoneStatic` | ⏳ |
| 3 | Missing-person poster of Seneca | `Diagnostic.MissingPersonPoster` | ⏳ |

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

---

## Known wrinkles to flag

- **Item 3 poster vs item 2 gate:** the pole is hidden until `SeenTornadoWarning`, but the poster is always present. The area is reached late so both normally render together; gating the poster is a one-line follow-up if it reads oddly.
- _(more added as encountered)_

## Verify-in-game steps

_(added per item)_

---
**Not merged — waiting on your verification.** Green tests + screenshots got each item this far, but nothing lands on `June17` until you play it.
