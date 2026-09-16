# Nightly report — 2026-09-15 — Dialogue plate always in view

Branch: `overnight/2026-09-15` (from `ImproveNPCText`). Not merged, not pushed.
Trello: dpGSoWAv "Improve NPC text placement so it's always visible when interacting."

## Result: DONE — all acceptance criteria green

| Check | Result |
|---|---|
| `Regression.DialoguePlateInView` (headed) | PASS |
| `Regression.HappyPath` (headless) | PASS |
| `Diagnostic.RickDialoguePrefix` (headed) | PASS |

Measured values from the final headed run (viewport 1518x883):

| Step | Value |
|---|---|
| Far (RickApproach, anchor 150cm): plate vs anchor | 0.00cm |
| Far plate projected height | 555px |
| Close (80cm): plate center | (0.333, 0.286), rect top 0.006 / bottom 0.565 |
| Close plate projected height | 497px (ratio 0.90, tol ±25%) |
| Look-away 120°: plate vs anchor | 0.00cm |

Screenshots (also copied to `Tests/E2EGoldens/`):
- `Saved/Screenshots/WindowsEditor/E2E_DialoguePlateInView_Far.png` — plate at anchor, readable.
- `Saved/Screenshots/WindowsEditor/E2E_DialoguePlateInView_Close.png` — full line
  "I'll meet you inside once I'm done" readable next to Rick's head. Before the
  fix the same shot showed only the bottom sliver of the text at the top edge.

## What changed

**New `UDialoguePlateComponent`** (`Source/weirdplace2/DialoguePlateComponent.h/.cpp`),
added via `CreateDefaultSubobject` on Rick, Seneca, Hudson. It owns the
`WorldSpace_UI_Dialogue` lookup + Transparent blend mode (previously copy-pasted
x3), the per-tick billboard (previously copy-pasted x3), and the new clamp.
Rick and Hudson no longer tick at all; Seneca's Tick keeps only its smoking logic.
`DialogueWidgetComponent` on each NPC is still populated (CarRideComponent reads it).

Clamp rule: while the NPC's skeletal-mesh bounds center is inside a 60° cone
around the camera forward, project the authored anchor. If its center is outside
the 15..85% band **or the draw rect isn't fully on screen**, deproject the
clamped screen point at `min(anchorDist, 60cm)` and scale the widget by
`newDist / 150cm` so it reads at conversational-distance size. Otherwise (and
whenever the NPC is out of the cone) it rests at the anchor. Movement/scale are
`VInterpTo`-smoothed. The anchor is cached *relative to the widget's attach parent*
so the car ride's windshield relocation of the child actor stays transparent.

Tunables (`weird.DialoguePlate.*`): `SafeMinFrac` 0.15, `SafeMaxFrac` 0.85,
`MinCameraDistance` 60, `ReferenceDistance` 150, `InterpSpeed` 10,
`ConeHalfAngleDeg` 60, `Debug` false (per-tick log of the clamp math).

**Harness**: `UTestDriverSubsystem::GetDialoguePlateScreenState`,
`FTD_AssertDialoguePlateOnScreen` / `AtAnchor` / `CaptureDialoguePlateHeight` /
`AssertDialoguePlateHeightRatio`, `FTD_TeleportNearRick` gained a distance param.

## Deviations from the plan (worth a look)

1. **Center-in-band alone was not RED.** At 80cm the plate *center* was already at
   (0.30, 0.18) — inside the band — while the text was clipped, because the widget
   draw rect is 1000x500 with the text at its top and the rect was 1104px tall on
   an 883px viewport. The on-screen assertion (and the clamp trigger) now also
   require the projected rect's top/bottom inside [0, 1]. The plan's RED run failed
   on the height-ratio criterion instead; after tightening, it fails on the
   on-screen one as intended.
2. **Added `ReferenceDistance` (150cm).** The plan scaled by `newDist/anchorDist`,
   which preserves the *current* apparent size — 2x at 80cm — and fails criterion 3.
   Scaling relative to a fixed reference distance is what makes close-range size
   match far-range size.
3. **Cone test uses the skeletal mesh bounds**, not `GetComponentsBoundingBox`:
   Rick's whole-actor bounds center lands *behind* the camera (some component's
   bounds are far off), so the clamp never engaged.
4. The DialoguePlateInView test needs `-Headed` (no viewport under NullRHI). It
   fails loudly headless ("viewport size is zero"), same as the other screenshot
   regressions.

## Open questions for the morning

- The text sits at the *top* of a 1000x500 draw rect, so the clamped plate lands
  fairly high on screen (rect top at 0.006). Shrinking the widget's DrawSize to
  hug the text (a Blueprint edit in `WorldSpace_UI_Dialogue`) would let the clamp
  center the text more naturally. Not done — asset change, outside scope.
- Seneca and Hudson get the same behaviour untested beyond HappyPath. Worth a
  quick walk-up in the editor.
- Trello card not moved; do that after review.

## Commits

See `git log ImproveNPCText..overnight/2026-09-15`.
