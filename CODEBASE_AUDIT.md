# Codebase Audit — 2026-07-03

Six parallel deep-read audits (gameplay correctness, UI/components, performance, architecture, E2E infrastructure, repo/config hygiene) over all ~27k lines of `Source/weirdplace2/`, followed by a verification pass: **every HIGH finding below was independently re-verified against the source** — quoted lines are real, not agent hallucinations.

## The honest verdict

This is in considerably better shape than "vibe coded, never read the code" usually produces. There's a real interface (`IInteractable`) used consistently by all 8+ interactive classes, a genuine door class hierarchy, correctly world-scoped subsystems, near-universal `UPROPERTY` GC hygiene in the UI layer, weak-lambda timers in the newer code, async asset loads with rationale comments, and an E2E harness that has institutionalized a year of hard-won lessons. The agent has clearly been held to conventions.

The debt concentrates in exactly three places:

1. **Copy-paste families that drifted apart.** The Menu/Inventory/Keypad UI components are the same ~400-line state machine pasted three times; the three NPCs duplicate all their dialogue plumbing; MovieBox/InspectablePickup duplicate inspection. Bug fixes landed in whichever copy was touched last — there are now at least four verified bugs that exist *only because* a fix didn't propagate to the sibling.
2. **Dead systems left in place.** ~900 lines of `InventoryRoomComponent` (+ friends) that nothing references, an unreachable story beat in Seneca, dead dialogue-options plumbing. Dead code is worse for you than for most teams: a future agent greps "inventory", finds the dead system, and extends the wrong one.
3. **The oldest code predates the conventions.** `AMovieBox` — the single most-instantiated class in the game (~1,080 spawned instances) — still has class-wizard defaults (tick always on, raw un-`UPROPERTY`'d pointers) that everything written later does correctly.

Plus one strategic finding that isn't about code at all: **the VR target is currently fiction** (§6).

---

## 1. Top 10 actions, ranked by value

| # | What | Why | Size |
|---|------|-----|------|
| 1 | De-tick the MovieBox fleet | ~3,200 no-op tick dispatches/frame, est. 1.5–3 ms | S |
| 2 | Fix the key-break wedge guard | Can permanently dead-end the story | S |
| 3 | Fix `TeleportTriggerBox` "UltraDynamic" needle | Feature destroys the wrong actor | S |
| 4 | Delete the dead `InventoryRoomComponent` cluster | 900-line agent trap w/ hitch grenade inside | S |
| 5 | ✅ ~~Fix Rick's missing `OnDialogueEnded` dispatch~~ (done 2026-07-04, `5a5babeb`) | Real bug born from NPC copy-paste | S |
| 6 | Decide the combined-tape beat: cut or rewire | ~120 lines of unreachable story code | S–M |
| 7 | ✅ ~~E2E loop economics (§5): step-delay, timeout, fail-fast~~ (E1–E3 done 2026-07-04, `b5b13096`; E4–E8 still open) | Directly buys back overnight agent hours | S–M |
| 8 | Decide VR for real (§6) | Everything downstream depends on it | strategic |
| 9 | Fix the "this PC dies" exposure (§7) | 8.9 GB of level content exists only on this machine | S |
| 10 | ✅ ~~Merge `AMyCharacter` → `AFirstPersonCharacter`~~ (done 2026-07-04, `fa7cec27`) | Two names for one object; already confused the agent's own memory once | M |

---

## 2. Bugs (all verified)

### HIGH

**B1 — Key-break re-entrancy guard can wedge the story permanently.** `OutsideBathroomDoor.cpp:159` sets `bKeyBreakInProgress = true` *before* six validation early-returns (lines 163–196: null `MyCharacter`, `Inventory`, `KeyLockSocket`, `AnimKeyMesh`, `FullKeyMesh`, `KeyInsertTimeline` — all BP-assigned and unset-able). If any fails, the guard stays armed forever and `Interact_Implementation` (line 86) ignores every future interact on the door — no rattle, no retry, `OnKeyDropped` and the Seneca-smoking chain never fire. Fix: arm the guard only after validation passes (just before `Inventory->RemoveItem` at line 209).

**B2 — The combined-tape beat is unreachable; state machine has dead states.** `Seneca.cpp:911` goes `WaitingForBlankTape → ReadyToGiveKey` directly ("burn off-screen"). Nothing anywhere (Source or Blueprints — both grepped) assigns `AwaitingTapeBurn`, so `GiveCombinedTape()` (Seneca.cpp:1016) can never pass its state guard, `CombinedTapeDef` is never granted, and `TakenMovies` (whose header comment promises the player's movies come back) is only cleared inside the dead path. This reads like an intentionally cut beat that left its skeleton behind. Decide: rewire the burn trigger, or delete the two dead states + ~120 lines of handlers so no future agent "helpfully" extends them.

**B3 — `DestroyUltraDynamicActors` destroys the wrong actor and misses its targets.** `TeleportTriggerBox.cpp:147` matches `ClassName.Contains(TEXT("UltraDynamic"))`. UDS actors are classed `Ultra_Dynamic_Sky_C` / `Ultra_Dynamic_Weather_C` (with underscores — your own `UltraDynamicWeatherController.cpp:15` uses the underscore needle) → **no match**. But `UltraDynamicWeatherController` *does* match → your storm-fade controller gets destroyed instead. With `bDestroyUltraDynamicActors=true` the feature does the exact opposite of its comment.

**B4 — Rick's end-of-dialogue callback silently never fires.** The end-dispatch is hardcoded per-NPC in two places that disagree: `AdvanceSimpleDialogue` (FirstPersonCharacter.cpp:1246) handles Seneca and Hudson only; `AdvanceDialogue` (:1237) handles all three. Rick uses simple dialogue for his idle lines (Rick.cpp:308), so `Rick::OnDialogueEnded` — and `OnRickDialogueEnded.Broadcast()` — is skipped on that path. Best fixed structurally: put `OnDialogueEnded()` on the `IDialogueWidgetProvider` interface and delete both cast chains (§4).

### MED

**B5 — Five systems share one legacy `"Exit Interaction"` binding; unbind removes first-match, not their own.** Binders: `InspectablePickup.cpp:134`, `MovieBox.cpp:247`, `PayPhone.cpp:193`, `InventoryUIComponent.cpp:356`, `KeypadUIComponent.cpp:364` — all on the same PlayerController InputComponent, all removed via `RemoveActionBinding("Exit Interaction", IE_Pressed)` which strips the *first* match. The inventory deliberately keeps its binding live through the 0.3s close animation, so overlap is reachable (e.g., keypad opens while inventory is closing) → one system strips the other's exit handler → stuck state. Fix: store and remove the exact binding handle, or migrate to Enhanced Input like the nav actions.

**B6 — MovieBox/InspectablePickup raw pointers without `UPROPERTY`.** `MovieBox.h:59–63` (`InspectedActor`, `PlayerController`, `MyCharacter`) — GC never nulls these; `CanInteract()` (MovieBox.cpp:181) trusts a potentially stale `MyCharacter`. `InspectedActor` is also never initialized in the MovieBox constructor (read-before-set in `IsBeingInspected()`). Same shape in `InspectablePickup.h:71–75`. Fix: `UPROPERTY()` (or `TWeakObjectPtr`) + initialize.

**B7 — Raw-`this` lambda timers, not cleared in EndPlay.** `FirstPersonCharacter.cpp:882` (item notification, 3s) and Seneca's `HideKeyHandle`/`HideBasketHandle` lambdas. Bare-lambda timers are NOT auto-invalidated when the actor dies → use-after-free if destroyed mid-timer. `MovieBox.cpp:238` already does it right with `CreateWeakLambda` — copy that pattern.

**B8 — DoubleDoor's locked path "opens" without moving the leaves.** `Door.h:48` documents that subclasses override `ApplyOpenAmount`; `ADoubleDoor` never does. Its locked/keypad path defers to `Super::` which animates the parked, hidden static `DoorMesh` → `Opened == true`, sound plays, leaves never move. Latent (double doors ship unlocked today) but armed the moment anyone checks `IsLocked` in the Details panel.

**B9 — `UI_Dialogue::Close()` never cancels the typewriter timer.** `UI_Dialogue.cpp:104` collapses the widget but `SetNextDisplayTextCharacter` keeps rescheduling every 0.03s — per-character blip sounds keep playing after the dialogue closes, to the end of the line.

**B10 — `TimedVisibility` toggle is inverted.** `TimedVisibility.cpp:73–80`: `IsVisible` semantics flip after the first toggle — first toggle is a visual no-op (doubled first period), and `SetActorTickEnabled(IsVisible)` thereafter enables tick while hidden, disables while visible (backwards of its comment). Also uses `SetActorHiddenInGame`, which your own CLAUDE.md bans as unreliable. Live: `MoviePoster.uasset` uses this component.

**B11 — Rick's money-give line index can silently die.** `Rick.cpp:154` resets `MoneyGiveLineIndex = INDEX_NONE` on load; if `RickGivesMoney.txt` has no `[Action]` cue, money is never given and Seneca's `WaitingForMoney` can never be satisfied — a full progression wedge with only a Log-level breadcrumb. Add a `UE_LOG(Error)` when the loop ends with INDEX_NONE.

**B12 — Menu build aborts entirely on one missing material.** `MenuUIActor.cpp:187–192`: missing `M_SolidColor` early-returns before `BuildPausePage/ApplyPageVisibility` etc. → all three menu pages render stacked on top of each other. The error log is right; the abort scope is wrong (skip only the background).

**B13 — Inventory highlight MID bug: fixed in the keypad copy, never backported.** `InventoryUIActor.cpp:268` assigns the shared material asset and nulls `SelectionMaterial` — so the fade-with-animation path (`Cast<UMaterialInstanceDynamic>` at :172) is a permanent silent no-op and the "re-apply material" recovery code is unreachable. `KeypadUIActor.cpp:177` — a copy of this class — wraps it in a MID properly. Textbook copy-paste divergence.

**B14 — Silent fallback chains (the pattern you've banned).** Worst offenders: the four-layer thumbnail fallback ending in a *log-nothing* hash-colored placeholder (`InventoryUIActor.cpp:389–453` — the exact chain behind the packaged-build checkerboard incident); keypad-door-without-keypad falls back to plain locked sound with no log (`Door.cpp:93–98`); no-InventoryUI falls straight into the key-break sequence, skipping the give beat, no log (`OutsideBathroomDoor.cpp:136–139`); `SpawnerActorComponent.cpp:22–35` logs null-Owner then dereferences it anyway. Fix per your own rule: one canonical path, `Error` + unmistakable failure state otherwise.

### Notable LOWs (full agent detail preserved in git history of this file's sources)

- `MovieBox.cpp:56` BeginPlay early-return leaves the floating interact prompt visible forever on a misconfigured box.
- `PayPhone.cpp:25` "static + voices" texture silently dead (default loads commented out, no log when unset).
- `Seneca.cpp:980` money `RemoveItem` result ignored (its blank-tape sibling checks it).
- `SpawnerActorComponent.cpp:156` named spawns use `Required_Fatal` → a second spawner in the level is a hard crash.
- `BladderUrgencyComponent` EndPlay lacks the `GetWorld()` null-guard its keypad sibling has; `WarmupTimerHandle` never cleared.
- Door auto-close logs at Display every 0.2s per open door; every E-press logs a Warning with two FStrings — log spam that buries real warnings.

---

## 3. Performance (VR lens: 11 ms budget)

**P1 — HIGH: ~1,080 `AMovieBox` actors tick every frame, forever.** `MovieBox.cpp:30` ships the class-wizard default (`bCanEverTick = true` — the template comment "You can turn this off to improve performance" is still attached, chef's kiss). The spawner instantiates ~1,080 (verified against the live editor: 15/shelf × 6 shelves × 12 bookcases). The tick body matters only while *one* box is being inspected. Each box also carries an always-ticking `UDiegeticTextComponent` (early-outs when invisible, but still dispatches) and a `UWidgetComponent` (engine default ticks) → **~3,200 registered tick functions**, estimated **1.5–3 ms/frame of pure dispatch overhead doing nothing**. That's 15–25% of a VR frame budget. Fix is the pattern your newer code already uses (`CarRideComponent`: `bStartWithTickEnabled = false`, enable on inspect, disable on stop). Measure with `stat game` before/after — this is the only game-code cost that plausibly moves the needle.

**P2 — Dormant hitch grenade in dead code.** `InventoryRoomComponent.cpp:522` does a full-viewport `ReadPixels` (GPU flush) + 31-tap CPU Gaussian blur over every pixel on the game thread — hundreds of ms — synchronously from `TeleportToInventoryRoom`. Nothing instantiates the class (verified: zero refs in Source *and* Content). Delete it before someone wires it up (§4).

**P3 — Three NPCs billboard invisible dialogue widgets every frame** (`Seneca.cpp:569`, `Rick.cpp:65`, `Hudson.cpp:60` — identical code): `SetWorldRotation` dirties render state per frame for a widget invisible ~99% of playtime. Ironically `UDiegeticTextComponent` already implements the same billboard *with* a visibility gate. µs-scale, but should be zero.

**P4 — Per-frame churn in the character tick:** three heap `TArray`s constructed per raycast call (`FirstPersonCharacter.cpp:992–999`), `FindComponentByClass<ULookAtPlayerComponent>` run twice per NPC hit, `GetItems()` returns the array by value each frame while inventory is open, and a Slate focus *diagnostic* (:304–325, string-building on focus change) still lives in the shipping tick. All trivially hoistable.

**P5 — Misc:** `SpawnerActorComponent` ticks from level start and builds two diagnostic FStrings per frame while the chord beat runs (`:95–96`) for data only E2E reads; `GazeReward`/`TimedTeleport`/`TimedVisibility` poll per-frame where a 0.1–0.2s timer loses nothing; the three UI components tick no-op switches while Closed (they gate their spawned *actors* correctly but never themselves).

**What's already good:** everything recent uses enable-on-demand tick (CarRide, Bladder, WeatherController, PerfWalk, UI actors); one crosshair trace/frame (fine); geometric gaze tests instead of trace spam; async cover loads; no per-tick asset loads; no debug draws; no GetAllActorsOfClass in hot paths. After P1, game code is a rounding error next to the already-diagnosed PSO/Nanite dev-env costs.

---

## 4. Architecture

**A1 — Delete the dead `InventoryRoomComponent` cluster** (`InventoryRoomComponent.*`, `MovieBoxDisplayActor.*`, `InventoryItemMapping.h`): ~900 lines, self-labeled DEPRECATED, zero references in Source or Content (verified by binary grep). It's the single biggest agent trap in the repo — plausible-looking "inventory" code that also contains P2's hitch and `/Game/Blueprints` path loads outside the cook list. Also dead (verified zero callers): `ABathroomKey` (whole class), `Seneca::IsPlayerLookingAtMe/StopSmokingAnim/GetDialogueLinesForCurrentState`, the empty `CheckMovieCount` + its delegate binding, `AMyCharacter::AddItemToInventory*` (BlueprintCallable — confirm no BP use), UI_Dialogue's dialogue-options plumbing (DlgSystem removed April 2026), `AMyCharacter::Tick` (empty override).

**A2 — One player, two names.** `AFirstPersonCharacter : AMyCharacter` — both live, only subclass, only pawn. Callers juggle both views of the same object (Hudson.cpp:102 casts the same pointer to both classes back-to-back); 68 casts across 19 files split between the two names; your own memory notes record the API on "AMyCharacter" while everything fetches AFirstPersonCharacter. No second pawn is ever coming (VR reuses this pawn). Merge down, delete `MyCharacter.*`, add a CoreRedirect for BP references, run the regression suite.

**A3 — The modal-UI triplets.** `UInventoryUIComponent`/`UKeypadUIComponent`/`UMenuUIComponent` are line-for-line copies: identical state enums, identical tick state machines, identical easing math, identical freeze/unfreeze, identical UPROPERTY blocks. Fixes drift (B13). Extract a `UWorldPanelUIComponent` base (~200 lines, deletes ~400) and — bigger win — replace the **five** copies of the menu→keypad→inventory priority chain in FirstPersonCharacter (:455–573) with one `GetActiveModal()` router. The spawned *actor* halves are genuinely different; leave them.

**A4 — The NPC dialogue plumbing triplets.** `GetDialogueWidget`, the blend-mode fix (which you had to apply three times — your memory file literally says "the 3 speaker cache sites"), the billboard tick, and the tricky "armed beat" double-broadcast pattern each exist ×3 across Seneca/Rick/Hudson. Plus the player hardcodes NPC types in the end-dispatch (B4). Fix order: add `OnDialogueEnded` to the interface (S, fixes B4), then extract a `UDialogueComponent` from FirstPersonCharacter (~300 lines: both engines — which are ~90% identical, "simple" is just per-line with a constant speaker — plus `FSimpleDialogueLine`, which currently forces Rick.h to include the whole character header).

**A5 — Explicit don'ts** (things that look refactorable but aren't worth it): the door hierarchy is real inheritance, not copy-paste — leave it; don't unify the three UIActor render classes; don't remove the in-class test seams (`*ForTest` methods are the backbone of the E2E loop); UStorySubsystem's world-scoped flags are correct by design; MovieBox/InspectablePickup inspection dedup is defensible but only 2 copies — defer.

**Good bones worth naming:** consistent `IInteractable` usage with a single dispatch site; forward-declaration discipline in headers (3 violations: MovieBox.h, Rick.h, FirstPersonCharacter.h's `Inventory.h`); subsystems used for the right things; no game-instance singletons.

---

## 5. E2E / overnight-loop economics

The harness is genuinely strong (first-tick-anchored timeouts, NullRHI screenshot handling, per-test PIE isolation, whole-run PASS validation, mostly-real assertions). Its problems are *economic*, and they tax every overnight session:

**E1 — A hidden 0.5s tax on every test step.** `CVarE2EStepDelay` defaults to 0.5s applied after *every* latent command (`E2E_LatentCommands.h:40,126`); nothing ever sets it to 0 (verified: no `.ps1` touches it). Across 22 regression tests that's plausibly **10+ minutes of pure sleep per suite run**. Make 0 the default; let `-Headed` review runs opt into 0.5.

**E2 — The 60-min watchdog has been outgrown and dies dumb.** `run_e2e.ps1:31` still assumes "9 tests × 2–5 min" — there are **22** regression tests (verified); a fully *passing* suite can be watchdog-killed. Worse, on timeout it kills and exits (:75–77) **without parsing the log**, so the agent learns nothing about which test hung. Scale timeout by matched-test count; always fall through to log parsing after kill.

**E3 — Fail-slow, artifact-free failures.** `AddError` doesn't abort the queue, so post-failure commands burn their full timeouts and cascade errors bury the root cause; there's no screenshot-on-failure. Wire `FTD_Base` to short-circuit once `HasAnyErrors()`, snap one failure screenshot + a state line (the status plumbing already knows the current step), and add a belt-and-braces per-command max age (one command — `FTD_LookAtWaypoint`, `E2E_LatentCommands.h:260` — can retry literally forever; its siblings AddError-and-finish).

**E4 — The single-TU monolith.** All 33 tests live in one 1,871-line cpp including a 4,160-line header (there's a `// force rebuild` comment hack at the top, which says it all), because one `TAutoConsoleVariable` static forces single-inclusion. Move the CVar + bodies into an `E2E_LatentCommands.cpp`, split tests into per-feature cpps → every test edit stops costing a full monolith rebuild. Also ~two-thirds of the header is 20-line boilerplate wrappers around single driver calls — a generic lambda-based `FTD_Step`/`FTD_Assert` pair would halve it and make new tests one-liners for the agent.

**E5 — Known-bogus runs aren't refused.** The worktree false-failure mode is documented in your memory but `run_e2e.ps1` happily runs from one (a worktree exists right now under `.claude/worktrees/`). Three lines: compare `git rev-parse --git-dir` vs `--git-common-dir`, refuse loudly.

**E6 — ~90 fixed sleeps** (`FTD_Delay`) where condition-waits already exist (e.g. `E2E_Steps.h:64` sleeps 0.6s for an animation despite `IsInventoryFullyOpen()` existing). Flake source + dead minutes; convert the worst offenders opportunistically.

**E7 — Shipping hygiene:** `UTestDriverSubsystem` (1,624 lines, `EnterKeypadCode`, `SetStoryFlag`, ...) has no build guard and no `ShouldCreateSubsystem` override (verified) → instantiates in every world including a shipped game's. The E2E test bodies themselves are properly stripped (`WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR`). Dormant, not exploitable — gate with `!UE_BUILD_SHIPPING` before any public build (PerfWalk includes it at runtime by design, so use `!UE_BUILD_SHIPPING`, not `WITH_EDITOR`).

**E8 — Machine-readable results:** the log-regex parsing is coupled to UE's log wording; UE's automation controller supports `-ReportExportPath` (per-test JSON + durations). Emit it alongside — stable interface for the agent, plus "which test got slower" across nights.

Also: `TestDriverSubsystem` and `WeirdplaceCheatManager` contain near line-for-line duplicate item-granting/asset-scanning code (the cheat copy handles cooked builds *better*); have cheats delegate to the driver. And two Regression tests are near-assertion-free (InventoryThumbnails asserts only a count — the actual feature is verified by a human comparing PNGs, which a headless PASS doesn't cover; InventoryFromStart is open/screenshot/close).

---

## 6. Strategic: the VR target is currently fiction

`weirdplace2.uproject` enables **no XR plugin** — no OpenXR, no stereo config, no `vr.*` cvar anywhere in Config/ (verified). The `GEngine->XRSystem->IsHeadTrackingAllowed()` gates in code are permanently false because XRSystem is null. Meanwhile the renderer is committed to Lumen GI + VSM + TAA with `r.AllowStaticLighting=False` — a stack that will fight a headset port hard (and the static-lighting escape hatch VR titles usually need is explicitly locked out). Every design decision (diegetic UI, VR-gated camera effects) is paying an ongoing tax for a target the project can't currently render one frame of.

Recommendation: pick one deliberately. Either (a) enable OpenXR now, get a headset frame rendering, and start letting real stereo profiling shape rendering decisions early — the longer Lumen/VSM assumptions bake in, the more expensive the reckoning; or (b) declare this a flatscreen game with VR-friendly *design sensibilities* and delete the phantom gates. Both are fine; drifting isn't.

---

## 7. Repo & config hygiene

**R1 — The "this PC dies" scenario is the one realistic catastrophe.** Three exposures compound: (a) 8.9 GB of *level-referenced* content is gitignored (`.gitignore:101–103`: BUILDINGS/VOL3_Attachments, Roadside/VOL2, Suburban_Household) — a fresh clone has missing refs and a failing cook, and the re-acquisition path (Fab redownload?) is documented nowhere; (b) `scripts/local/retarget_mocap_to_metahuman.py` is CLAUDE.md-documented but gitignored — exists only here; (c) 50 of your 69 stashes contain 993 binary asset states whose LFS objects are never pushed (prune-safe locally — verified against git-lfs 3.6.1 — but unrecoverable if the disk dies). Fixes: document the Fab re-acquisition path (or bite the LFS bullet), promote the retarget script to `scripts/`, triage stashes into pushed branches or drop them.

**R2 — Cook-list gaps, the exact failure mode that already burned you.** `DefaultGame.ini:19–23` covers 5 directories; missing: `/Game/Inventory` (cheat-manager + test-driver DA_* path loads — present in Development cooked builds, i.e. every Steam Deck deploy) and `/Game/Blueprints` (only referenced from the dead InventoryRoomComponent — moot after A1, otherwise add it). Two lines of insurance. All *live* runtime path-loads verified covered otherwise; the full inventory of every hardcoded `/Game/...` path in C++ is in §2/B14's sources — any new path outside the five covered dirs silently regresses packaging.

**R3 — Config debris:** ~60 lines of orphaned DlgSystem settings whose section header was deleted are now being parsed as keys of `[AndroidFileServerRuntimeSettings]` (`DefaultEngine.ini:130–188`) — harmless until an ini merge isn't; `DefaultNodeToCode.ini` for a removed plugin; `[HTTP] HttpActivityTimeout=3600` — a NodeToCode-era LLM-streaming timeout now applying to all runtime HTTP; `ProjectName=First Person BP Game Template` (shows in window title + crash reports); dead SimpleMap reference; duplicated `ExtendDefaultLuminanceRange`; Dialogue directory staged via two mechanisms (pick one); `DefaultDeviceProfiles.ini:9` forces SSR reflections for *all* Windows machines including shipped high-end (comment says "dev/low-end" — intent and effect disagree).

**R4 — Repo bloat, contained but real:** `.git` is 51 GB (50 GB local LFS store — `git lfs prune` reclaims); ~1.2 GB of pre-LFS binaries baked into plain-git history (75 MB .dna files etc. — not growing, fixable only by history rewrite, probably not worth it solo); `pureref.pur` (7.9 MB, actively edited, plain-git → ~8 MB of undeltifiable history per commit — add to LFS or ignore); `NIGHTLY_REPORT.md` is tracked *and* ignored (ignore is a no-op for tracked files → permanent diff noise; `git rm --cached`); tracked junk (`replay_pid*.log`, `.DS_Store`, `desktop.ini`, a `.lnk` shortcut, stale `lfs_files.txt`); 41 local + ~52 remote branches of dated WIP.

**Clean bills of health:** LFS coverage at HEAD is airtight (0 unsmudged pointers, all 6,297 uasset/umap tracked); Build/Target files disciplined (V6, IncludeOrder 5_7, editor deps properly gated); CREDITS.md exists and covers the big packs; all CLAUDE.md-referenced scripts except the retarget one exist and are tracked.

---

## 8. Making the overnight vibe-coding loop better

What you've built (CLAUDE.md conventions + memory files + E2E-verified overnight runs) is already the right architecture. The highest-leverage upgrades, in order:

1. **Kill dead code aggressively (A1).** Dead systems are disproportionately expensive for AI agents — they pattern-match into extending the wrong one, and every grep result costs context. This audit found the agent's own past leavings (`InventoryRoomComponent`, `BathroomKey`, dead Seneca states) are now its biggest future hazard. Consider a standing rule in CLAUDE.md: *"When replacing a system, delete the old one in the same PR."*
2. **Make the E2E suite cheap and honest (§5, E1–E6).** Step-delay to 0, timeout scaled, parse-after-kill, worktree refusal, fail-fast with a failure screenshot. Every one of these turns wasted overnight hours into productive ones. The single-TU split (E4) is the biggest compile-time win available.
3. **Add a system map to CLAUDE.md.** ~15 lines: "player = AFirstPersonCharacter (extends AMyCharacter — merge pending); modal UI = the three *UIComponent classes on the pawn; dialogue lives on the character, NPCs provide widgets; movie fleet spawned by SpawnerActorComponent." Agents currently re-derive this every session; the architecture agent needed ~200k tokens to map what 15 lines could state. (This audit's §4 can seed it.)
4. **Propagation rule for the copy-paste families.** Until A3/A4 land, add to CLAUDE.md: *"Menu/Inventory/Keypad UI components and Seneca/Rick/Hudson dialogue plumbing are triplets — any fix to one must be applied to all three or explicitly justified."* Four of this audit's bugs are propagation failures.
5. **Periodic cooked-build smoke.** Several failure classes here only exist in packaged builds (cook-list gaps, path loads, the checkerboard incident). A monthly `-cook` + 5-minute Deck session catches what no PIE test can.
6. **Stash/branch hygiene as an overnight chore.** "idk"-stash triage is exactly the boring, low-risk work an overnight agent does well: pop each on a scratch branch, summarize the diff, recommend keep/drop, you approve in the morning.

---

*Generated by a 6-agent parallel audit + manual verification pass, 2026-07-03. All HIGH findings re-verified line-by-line against source before inclusion.*
