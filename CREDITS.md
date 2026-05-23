# Credits & Third-Party Asset Attributions

This file tracks third-party assets used in weirdplace2 and the attribution / license requirements that come with them. Every external asset (sound, texture, mesh, music, font, code snippet) should be logged here when it lands in the project.

**Why this exists:**
1. Legal compliance — many free assets require attribution under their license (CC BY, CC BY-NC, etc.).
2. Audit trail — if an asset's license is later disputed or revoked, we know where it came from.
3. Shippable credits — entries here feed the in-game credits screen.

> **TODO markers:** Entries below were seeded from a Content folder scan. Anything marked _TODO_ needs human verification (exact license, pack name on Fab, commercial-use rights, etc.) before we can rely on it for shipping.

## Entry Format

```
### <Asset name or short description>
- **Author:** <Name / handle>
- **Source:** <URL>
- **License:** <e.g. CC0, CC BY 4.0, CC BY-NC 4.0, Fab Standard, custom>
- **Used in:** <where in the game / which .uasset(s)>
- **Required attribution:** <verbatim text the author asked for, or "none">
- **Links:** <author website, social, etc. — if requested>
```

---

## Audio

### "Journey To The Interweb" — Freesound #609250
- **Author:** Aemyn Connolly (@aemyn)
- **Source:** https://freesound.org/s/609250/
- **License:** Creative Commons 0 (public domain — no attribution legally required)
- **Used in:** `Content/Sounds/weirdsoundexperimentalsong.uasset` / `.wav`
- **Required attribution:** None legally, but author requested credit as "Aemyn Connolly". We honor that.
- **Links:**
  - Website: https://aemyn.bearblog.dev
  - Instagram: @aemyn

### Freesound #620404 — Passbys Inside City Tunnel
- **Author:** sotiris_laskaris
- **Source:** https://freesound.org/people/sotiris_laskaris/sounds/620404/
- **License:** _TODO — confirm from the freesound page_
- **Used in:** `Content/Sounds/620404__sotiris_laskaris__passbys-inside-city-tunnel.uasset`
- **Required attribution:** _TODO — depends on license_

---

## Visual Assets

### Fab — CRT TV
- **Source:** Fab marketplace
- **License:** _TODO — Fab Standard License (verify per-asset terms in your Fab library)_
- **Used in:** `Content/Fab/CRT_TV/`
- **Required attribution:** Fab Standard typically does not require per-asset credit, but verify.

### Fab — Laundromat
- **Source:** Fab marketplace
- **License:** _TODO_
- **Used in:** `Content/Fab/Laundromat/`, `Content/Laundromat/VOL2_Laundry/`

### Fab — Small Key (1MB)
- **Source:** Fab marketplace
- **License:** _TODO_
- **Used in:** `Content/Fab/Small_Key__1MB_/`
- **Notes:** Mesh authored at 57m scale; gameplay scales 0.001x. (See memory `reference_python_editor_apis.md`.)

### Fab — Suburban Household
- **Source:** Fab marketplace
- **License:** _TODO_
- **Used in:** `Content/Fab/Suburban_Household/`

### Quixel Megascans
- **Author:** Quixel / Epic Games
- **License:** Megascans are free for use in Unreal Engine projects under the Quixel/Epic license. Verify current terms.
- **Used in:** `Content/Megascans/3D_Plants/`, `Content/Megascans/Surfaces/`, `Content/Fab/Megascans/`, `Content/MSPresets/`
- **Required attribution:** None required for use in UE projects, but verify if shipping outside UE.

### Ultra Dynamic Sky
- **Source:** _TODO — likely Fab/Unreal Marketplace (Everett Gunther)_
- **License:** _TODO — confirm Marketplace EULA terms_
- **Used in:** `Content/UltraDynamicSky/`

### "VOL" Series Packs
These all follow a `VOL<N>_<theme>` naming convention typical of a marketplace creator. Confirm the exact pack/seller on Fab and update.

- `Content/Eighties/VOL4_Technology/` — _TODO seller/pack name_
- `Content/Fences/VOL2/` — _TODO seller/pack name_
- `Content/Vehicles/VOL3_RetroCars/` — _TODO seller/pack name_

### MetaHumans
- **Author:** Epic Games
- **License:** MetaHuman EULA — free for use in Unreal Engine projects. Cannot be used outside UE.
- **Used in:** `Content/MetaHumans/`
- **Required attribution:** None required, but the MetaHuman EULA applies.

### Epic-included content (no attribution required, listed for audit only)
- `Content/StarterContent/` — Epic Starter Content
- `Content/FirstPerson/`, `Content/FirstPersonArms/`, `Content/FPWeapon/`, `Content/ThirdPerson/` — Epic templates
- `Content/LevelPrototyping/` — Epic prototype materials

---

## Code / Plugins

### NodeToCode (modified)
- **Author:** protospatial
- **Source:** https://github.com/protospatial/NodeToCode
- **License:** _TODO — confirm upstream license_
- **Used in:** `Plugins/NodeToCode_5.4/` — Blueprint → C++ conversion during development
- **Modifications:** https://github.com/protospatial/NodeToCode/pull/14

### UnrealMCP
- **Source:** _TODO — confirm origin (likely a third-party MCP integration)_
- **License:** _TODO_
- **Used in:** `Plugins/UnrealMCP/`

---

## Fonts

_(none yet)_
