---
name: steam-deck-deploy
description: Build, push, and launch the game on the Steam Deck. Use when the user asks to deploy, push, or test on the Deck.
---

Build → `scripts/push_to_deck.ps1 -DeckHost deck@<ip>` → launch from Steam on the Deck (not directly — Steam Input has to wrap the process for the controller to work).

Build flags depend on what changed:
- New `UPROPERTY`/class → full `-cook -allmaps -build -stage` (or you'll hit `Bad export index` on cooked Blueprints)
- `.cpp` only → `-build -skipcook -stage`
- `.ini` only → `-skipbuild -skipcook -stage`

Full command + setup + log retrieval + Deck device profile notes: `docs/steamdeck-deploy.md`.
