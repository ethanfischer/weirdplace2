# Nightly report — 2026-09-15/16 — NPC dialogue: personal space, walk-away, text

Branch: `overnight/2026-09-15` (from `ImproveNPCText`). Not merged, not pushed.
Trello: dpGSoWAv "Improve NPC text placement so it's always visible when interacting."

## What happened

The first pass (commit b4ca3f69) clamped the plate into the viewport. You scrapped
that in the morning for a simpler model, implemented in the second commit:

1. **Player-only personal-space capsule on every NPC** — you can't walk into them.
2. **Dialogue no longer locks movement.** Walk past a distance and the dialogue
   freezes; interact with the same NPC again and it resumes at the frozen line.
3. **Text sits beside the head**, wrapped to a short two-line block (reference:
   your sheet-music-store screenshot).
4. **Text is rendered at 2x resolution** (no more pixelation up close / at 4K).

## Results

| Check | Result |
|---|---|
| `Regression.NpcPersonalSpace` (headed) | PASS — stopped 134cm from Rick's root |
| `Regression.DialogueWalkAway` (headed) | PASS — froze at 300cm, resumed same line |
| Full `Regression` suite (headed, 26 tests) | PASS |
| `Diagnostic.RickDialoguePrefix` (headed) | PASS |

Screenshots to look at:
- `Saved/Screenshots/WindowsEditor/E2E_NpcPersonalSpace_DialogueClosest.png` — the
  closest the player can get; two-line block beside Rick's head, crisp.
- `E2E_DialogueWalkAway_Frozen.png` / `E2E_DialogueWalkAway_Resumed.png`.

## What changed

**`UNpcPersonalSpaceComponent`** (new) on Rick/Seneca/Hudson. Spawns a capsule at
BeginPlay (runtime-created, not a default subobject: these NPCs get their root
from Blueprint and a native scene subobject would be adopted as root). Object
type Pawn, blocks only the Pawn channel, so the interaction trace, camera and
visibility all pass through. `weird.Npc.PersonalSpaceRadius` = 100 (player ends
~134cm from the root with the player capsule).

**Walk-away / resume** (`FirstPersonCharacter`): the `AddMovementInput` override
that blocked movement in dialogue is gone. `TickDialogueWalkAway` freezes any
dialogue once the player is `weird.Dialogue.WalkAwayDistance` (300cm, 2D) from
the NPC — skipped while movement mode is `MOVE_None`, which is how the car ride
parks the player, so the ride's gaze-gated dialogue is untouched. Freezing does
**not** call `OnDialogueEnded`, so NPC state machines stay where they were; the
lines, index, `bBlockNextDialogueAdvance`, and any pending `[Pause N]` are stored
per NPC. `HandleInteractTriggered` resumes a stored dialogue instead of calling
the NPC's `Interact`, re-showing the frozen line *without* re-broadcasting its
line-shown beat (so money/key/tape handoffs fire exactly once). A pause-before
on line 0 resumes by showing line 0 + broadcast; a pause-after resumes by
advancing.

**Text** (`UI_Dialogue` + `DialoguePlateComponent`):
- `weird.Dialogue.TextSize` 26 (authored px; speaker 1.5x), `WrapWidth` 420
  (replaces the `BackingTextWrapWidth` UPROPERTY), left-justified.
- `weird.Dialogue.RenderScale` 2: draw size, fonts, wrap, padding and feather are
  multiplied, the widget component scaled by 1/2. Same world size, 2x texels.
- Plate anchored at its left-middle edge (border slot anchor/alignment (0, 0.5),
  widget pivot (0, 0.5)). `DialoguePlateComponent` puts the child actor at
  `weird.DialoguePlate.OffsetUp` (170cm above root) and, every tick, steps the
  widget `OffsetRight` (20cm) along the **camera's** right axis before
  billboarding — so the line always starts beside the face and grows away from
  it, from any viewing angle. (Offsetting in the NPC's frame put the text through
  his head; that was the first attempt.)
- The authored per-NPC child-actor offsets (Rick's 36/175 etc.) are now
  overridden by the tunables at BeginPlay.

`UDialoguePlateComponent` keeps the dedup from the first pass (widget lookup +
Transparent blend + billboard, previously copy-pasted x3). Rick and Hudson no
longer tick.

**Harness**: `FTD_WalkForDuration` (feeds `AddMovementInput` in the control-yaw
frame), `FTD_AssertDistanceToActor`, `FTD_AssertPendingDialogueResume`;
driver `GetDistance2DToActor` / `AddWalkInput` / `HasPendingDialogueResume`.
The DialoguePlateInView test, its primitives and goldens are removed.

## Round 3 (2026-09-18) — car ride fixed, backing plate replaced by outline

- **Car ride restored.** `UDialoguePlateComponent::SetLayout` has two modes.
  `BesideHead` (default) steers the plate beside the head each tick with a
  (0, 0.5) pivot. `Centered` (the ride) leaves the widget where the ride parks
  it, pivot (0.5, 0.5), text top-center anchored at the authored offset with the
  original 700px wrap, centered justification, and no brightness tint (the ride
  authored its own dark text colour). `CarRideComponent` switches modes at ride
  start/end. Two engine gotchas bit here: `SetPivot` only writes the field, so
  the proxy needs `MarkRenderStateDirty`; and the authored top offset must be
  scaled by the render-scale factor.
  `Diagnostic.CarRideDialogue` screenshots the windshield line
  (`E2E_CarRideDialogue_Line1.png`) — it waits out the section's `[Pause 85]`,
  so it takes ~2.5 min.
- **Backing rectangle gone.** `UI_Dialogue` no longer wraps Text in a UBorder;
  the Text block gets a fresh auto-size canvas slot and a black font outline
  (`weird.Dialogue.OutlineSize` 2 authored px). The Backing* UPROPERTYs and the
  feather-material path are removed; `M_DialogueBackingFeather` is now unused
  (asset left in place — delete when convenient).
- **Less glow.** Rather than a new material asset, the widget's existing
  `TintColorAndOpacity` material parameter is set from
  `weird.DialoguePlate.Brightness` (0.45) — the engine pass-through material
  already multiplies emissive by it, so a custom material would have added an
  asset for no gain. Tune live if 0.45 reads too dim.
- Test rename: `SenecaTextBacking` → `SenecaTextOutline`
  (`FTD_AssertDialogueTextOutline`, driver `GetDialogueTextState`).
- One flake seen: `SenecaTextOutline` missed its interact once on the first run
  right after a build (state stayed FreeRoaming); passed on rerun and in the
  full suite.

## Things to check / decide in the morning

- **Personal-space radius vs level geometry**: Seneca behind the counter and
  Hudson weren't checked by eye. If a 100cm capsule pokes through a counter the
  player could get pinned; `weird.Npc.PersonalSpaceRadius` is live-tunable but
  applies at BeginPlay (PIE restart).
- **Character movement slides around the capsule.** Walking straight at an NPC
  and holding W carries the player around them. That's stock UE behaviour and
  reads fine, but it's why the test only walks for 0.5s.
- **Scripted-dialogue resume is untested end to end.** The mechanism is shared
  with simple dialogue (which is tested), and the beat-broadcast rules are as
  described above, but no test walks away mid-money-handoff. If you want one,
  it needs Seneca forced into `WaitingForMoney` first.
- Text/plate values (`weird.Dialogue.TextSize`/`WrapWidth`, `weird.DialoguePlate.
  OffsetRight`/`OffsetUp`) are tunables — dial in-editor, then bake.
- Trello card not moved.

## Commits

See `git log ImproveNPCText..overnight/2026-09-15`.
