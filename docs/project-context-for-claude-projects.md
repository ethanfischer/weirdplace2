# Weirdplace2 — Project Context

*(Context doc for brainstorming. The actual codebase lives on Ethan's PC at `C:\Users\ethan\repos\weirdplace2`; this is the design/world snapshot so you can riff on ideas without repo access. Last updated 2026-08-22.)*

## What the game is

A first-person exploration/vibe game built in Unreal Engine 5.8, planned for VR (currently developed flatscreen, also deployed to Steam Deck). Tone: liminal, uneasy Americana — a late-night road trip that stops at a lonely gas station / video rental store while a storm builds. Think point-and-click adventure pacing inside a Mothman-Prophecies-meets-VHS-store atmosphere. There is deliberate ambiguity and dream logic (a possible "other world" mechanic, see Open Design Questions).

**Core design principle: everything is diegetic.** No screenspace UI ever. Dialogue appears on floating plates near the speaker, inventory is a physical in-world UI, the keypad is a real keypad, tunables and effects live in the world. This is non-negotiable because of the VR target.

## The story so far (MVP happy path)

1. **Car ride intro** — you ride shotgun with your friend **Rick** at night. He monologues (small talk, complaining about his car). Your bladder fills; Rick pulls over at a gas station. Scenery streams past via a runtime "conveyor" of shrubs/telephone poles to sell motion.
2. **The gas station / video store** — clerk **Seneca** won't give you the bathroom key until you rent 3 movies (browse a shelf of VHS boxes, physically inspect and collect them). He charges **$2.47** but you have no money.
3. **Money** — Rick, outside at the pump, gives you cash if you ask. (Alt path being designed: **Hudson**, a homeless man outside, begs for change — if you give the money to Hudson you're broke; a "strange washer" found later covers it, Seneca weirdly accepts.)
4. **Blank tape fetch quest** — Seneca needs a blank VHS hidden somewhere in the collection to dub your movies onto. Find it, get the bathroom key. Dubbing "takes a while" — he goes out to smoke.
5. **The key breaks** in the bathroom door lock. While stuck, a **tornado warning** plays on a TV. The storm beat hits: lights dim, ambience silences, Rick disappears, pea-soup fog rolls in (`UStormFogComponent`, closes fog distance to near-black).
6. **The payphone** — a telephone is revealed; picking up the handset plays a recorded message containing the **keypad code to the employee bathroom** (design intent: you "write it down wrongly," enter it wrongly, but it works anyway — dream logic).
7. Enter through the employee hallway, keypad door, reach the stall. Relief. End of current MVP slice.

## Characters

- **Seneca** — the VHS store clerk. Dry, transactional, unhurried ("These things take time"). Has a smoking-break state. Design notes pending: cigarette tweaks, giving eye contact only after his friend talks to him.
- **Rick** — your friend/driver. Chatty, self-deprecating, generous ("You need cash? No worries, man. I got you"). Missing a gas-pump animation; his pose when looking at you needs work.
- **Hudson** — homeless man outside asking for change. The moral-choice pressure valve on the money beat.

Dialogue is one sectioned `.txt` per NPC in `Content/Dialogue/` (sections keyed to quest state, `[Tag]` action cues). LLM-generated movie comments exist but are slated for replacement with authored lines.

## Systems that exist (so ideas can hook into them)

- **Story flags**: `UStorySubsystem` (EStoryFlag progression) owns beat side effects — storm dim/hide/silence and station relight are driven by actor tags (`StormDimLight`, `StormHideActor`, `StormSilenceAmbient`).
- **Interaction**: `IInteractable` on world actors; player states FreeRoaming / Interacting / InSimpleDialogue / InDialogue gate input.
- **Inventory**: physical items (VHS tapes, money, key, broken key, blank tape, washer) with 3D inspection (grab, rotate, blurred background), diegetic slot UI.
- **Gaze rewards**: staring at certain things grants rewards (e.g., staring at a light grants cash). What gaze rewards *should* be is an open design question.
- **Payphone, keypad door, double doors, pause menu (diegetic), storm fog, tornado-warning TV** — all working.
- **E2E test harness** with golden screenshots covering the whole happy path, so any new beat should be testable end-to-end.

## Open design questions (great brainstorm targets)

- **Two worlds**: "There is no dying, only waking up in the other world." Two interleaved progressions — one where the drive continues, one at the gas station — eventually converging; player interprets the relationship. Mostly unexplored.
- **Death/stakes at the start**: e.g., Rick asks if he should turn off his headlights; say yes and he crashes.
- Make *every* environment item interactable, point-n-click style.
- The wrong-code-that-works payphone beat (writing the code down incorrectly).
- Hudson money path resolution (the strange washer).
- Gaze rewards: what should they be? TV gaze reward turning the TV off with CRT static?
- Camera roll / dutch angle as aesthetic (a debug accident tilted the horizon ~4° and it looked good — maybe for dread moments, the other world, or the drive).
- "Follow the money" (a high idea, meaning unknown even to its author).
- "Century Massage" — a place Seneca might mention; unbuilt reference.
- Replacing copyrighted movie content; more collectibles; richer soundscape ("Oasis" area especially); walking away mid-dialogue.

## Constraints to respect when proposing ideas

- **VR-first**: no screenspace UI; FOV-zoom/camera-motion effects must be flatscreen-only; post-process overlays (vignette) are VR-safe.
- Solo dev + Claude Code doing implementation; ideas should be scoped for one person. There's an established E2E/screenshot workflow, so "Claude-friendly" tasks (systemic, testable) get done fast; art/animation/sound tasks bottleneck on Ethan.
- Targets: PC (dev on a GTX 1070) and Steam Deck, so perf-heavy ideas need a sanity check.
- Credit third-party assets in credits.md.

## Sound effects collection

Ethan owns the "ALL IN ONE SOUND LIBRARY BUNDLE" (Gumroad, giant SFX pack) under `C:\Users\ethan\Music\ALL IN ONE SOUND LIBRARY BUNDLE\`. Files aren't all on disk, but a metadata CSV there is searchable (PowerShell `Import-Csv`). When proposing sound ideas, assume this library likely has a candidate — suggest search keywords for it.

## How to use this project from your phone

Ideas dropped here will later be handed to Claude Code on the PC. When responding to an idea, useful outputs are: how it fits the story beats above, which existing system it hooks into, rough scope (Claude-friendly vs needs-human art/sound), and a crisp task description Ethan can paste into `todo.md` or a Claude Code session.
