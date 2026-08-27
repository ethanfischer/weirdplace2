# Credits & Third-Party Asset Attributions

This file tracks third-party assets used in weirdplace2 and the attribution / license requirements that come with them. Every external asset (sound, texture, mesh, music, font, code snippet) should be logged here when it lands in the project.

**Why this exists:**
1. Legal compliance — many free assets require attribution under their license (CC BY, CC BY-NC, etc.).
2. Audit trail — if an asset's license is later disputed or revoked, we know where it came from.
3. Shippable credits — entries here feed the in-game credits screen.

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

### "Passbys inside City Tunnel" — Freesound #620404
- **Author:** sotiris_laskaris
- **Source:** https://freesound.org/s/620404/
- **License:** Creative Commons Attribution 4.0 (CC BY 4.0) — attribution required
- **Used in:** `Content/Sounds/620404__sotiris_laskaris__passbys-inside-city-tunnel.uasset`
- **Required attribution:** `"Passbys inside City Tunnel" by sotiris_laskaris -- https://freesound.org/s/620404/ -- License: Creative Commons Attribution 4.0`

### "Ambient alien sound 05" — Jagadamba (freesound)
- **Author:** Jagadamba
- **License:** Creative Commons Attribution 4.0 (CC BY 4.0) — attribution required
- **Used in:** `Content/Sounds/aliensound.mp3` / `.uasset`
- **Required attribution:** `"Ambient alien sound 05" by Jagadamba (freesound.org) -- License: Creative Commons Attribution 4.0`

### "Bathroom Room Tone" — danhelbling (freesound)
- **Author:** danhelbling
- **License:** Creative Commons 0 (public domain — no attribution legally required)
- **Used in:** `Content/Sounds/bathroomroomtone.wav` / `.uasset`

### "Trying to open a locked door" — Kodack (freesound)
- **Author:** Kodack
- **License:** Creative Commons 0
- **Used in:** `Content/Sounds/lockeddoor.uasset`

### "Brazilian_Waterfall_River" — SuperStudioBR (freesound)
- **Author:** SuperStudioBR
- **License:** Creative Commons Attribution 4.0 (CC BY 4.0) — attribution required
- **Used in:** `Content/Sounds/waterfall.wav` / `.uasset`, `Content/Sounds/waterfall2.mp3` / `.uasset`
- **Required attribution:** `"Brazilian_Waterfall_River" by SuperStudioBR (freesound.org) -- License: Creative Commons Attribution 4.0`

### "Long intense stomach growl 8" — SamanthaCastleberry (freesound) — *suspected*
- **Author:** SamanthaCastleberry
- **License:** Creative Commons 0
- **Used in:** `Content/Sounds/bladder.wav`/`.uasset`, `bladder2.wav`/`.uasset` (suspected — only stomach growl in user's freesound history; `bladder` naming is project-side gameplay context)

### "WavesAudioDoorOpenSFX" — MAVOM22 (freesound) — *suspected*
- **Author:** MAVOM22
- **License:** Creative Commons 0
- **Used in:** `Content/Sounds/dooropen.uasset` (suspected — only door-open sound in user's freesound history)

### "EAS Alarm" — mpaol2023 (freesound)
- **Author:** mpaol2023
- **Source:** https://freesound.org/s/370184/
- **License:** Creative Commons 0 (public domain — no attribution legally required)
- **Used in:** `Content/Sounds/tornadoalert.uasset` — looping tornado-alert siren blared diegetically from the store TVs during the tornado-warning storm beat

### "Convenience Store Door Chime" — zebragrrl (freesound)
- **Author:** zebragrrl
- **License:** Creative Commons Attribution 4.0 (CC BY 4.0) — attribution required
- **Used in:** `Content/welcomebell.wav` / `welcomebell.uasset` (and the related `SColor_GreetingBell.uasset` if it references this SoundWave)
- **Required attribution:** `"Convenience Store Door Chime (16bit, 44.1kHz, Stereo)" by zebragrrl (freesound.org) -- License: Creative Commons Attribution 4.0`

### Player footsteps — "UltimateRealism Footsteps Collection" (dpsounds)
- **Author:** dpsounds
- **Source:** https://dpsounds.gumroad.com/l/ultimaterealism (purchased 2026-08-27)
- **License:** Gumroad purchase; no license file ships in the zip — royalty-free game use per listing. Receipt retained.
- **Used in:** `Content/Sounds/Footsteps/<Set>/S_Footstep_<Set>_NN.uasset` — Carpet ×47 (pack Carpet), Tar ×27 (pack Concrete), Tile ×28 (pack Tile), WetDirt ×9 (pack MuddyGravel), all RMS loudness-matched on import. Played by `UFootstepComponent`: floor actors / volumes tagged `Footstep.<Set>` pick the set, untagged floors use the `weird.Footstep.Set` fallback (Tar).
- **Required attribution:** None required; credited anyway.

### Audio files with unconfirmed source
These project files have no obvious match in the user's freesound history. Could be original recordings, from a marketplace pack, or freesound files renamed beyond recognition. Verify and credit if third-party.

- `Content/Sounds/Wind.uasset`, `Content/Sounds/WindInside.uasset` (origin confirmed not freesound)
- `Content/Sounds/chord.wav` / `.uasset`
- `Content/Sounds/elevatormusic.uasset`
- `Content/Sounds/lel.uasset`
- `Content/Sounds/lowvoice.uasset` / `LowVoiceSoundCue.uasset`
- `Content/Sounds/Passing.uasset`
- `Content/Sounds/MenuOpen.uasset`, `MenuClose.uasset`, `MenuItemSelected.uasset`
- `Content/Sounds/ItemCollect/itemCollected_*.uasset` (11 variants)
- `Content/Sounds/TextBlip/textBlip_01.uasset`
- `Content/Sounds/Soundscape/*` (Color_Wind, Color_WindInside, Palette_Inside, Palette_Outside — these are likely engine SoundscapePalette/SoundClass assets, not raw imports)
- `Content/Sounds/SC_Ambient.uasset` (likely a SoundClass, not a raw import)

### License watch-list — commercial-use restrictions
_None at present. CC BY-NC assets have been removed from the project._

---

## Visual Assets

### Quixel Megascans
- **Author:** Quixel / Epic Games
- **License:** Megascans are free for use in Unreal Engine projects under the [Quixel/Epic license](https://quixel.com/terms). Cannot be used outside UE without a separate license.
- **Used in:** `Content/Megascans/3D_Plants/`, `Content/Megascans/Surfaces/`, `Content/Fab/Megascans/`, `Content/MSPresets/`
- **Required attribution:** None required for in-engine use.

### MetaHumans
- **Author:** Epic Games
- **License:** MetaHuman EULA — free for use in Unreal Engine projects. Cannot be used outside UE.
- **Used in:** `Content/MetaHumans/`
- **Required attribution:** None required.

### Fab marketplace assets — seller verification pending
The folders below came from Fab. The Fab Standard License typically does **not** require per-asset credit, but you should verify the specific license in your Fab library (each Fab listing shows its license tier on the product page). Fill in the seller name from your Fab library.

- `Content/Fab/CRT_TV/` — **Seller:** [timahene3d](https://www.fab.com/sellers/timahene3d) on Fab
  - **License:** Fab Standard (acquired via Fab My Library; license persists even though the product itself has since been delisted from the public storefront)
  - **Required attribution:** None required under Fab Standard.
  - Contents: `BP_TV`, `MP_TV*`, `M_TVScreen`, `crt_tv` mesh + 10 textures, `drivingvideo.uasset`
- `Content/Fab/Small_Key__1MB_/` — **Source: UNKNOWN** — not in user's Fab library despite the folder name. The `__1MB_` suffix suggests an auto-generated download from a site that includes file size in the filename (Sketchfab and TurboSquid both do this), but the actual origin has not been confirmed. Small key mesh, authored at 57m, gameplay scales 0.001x — see memory `reference_python_editor_apis.md`.
- `Content/Fab/Laundromat/VOL2_Laundry/` — **Seller:** Dekogon Studios (Fab Standard License). Same content also at `Content/Laundromat/VOL2_Laundry/`.
- `Content/Fab/Suburban_Household/VOL2_Couches/` — **Seller:** Dekogon Studios (Fab Standard License).

### "VOL" series packs (Fab marketplace) — Dekogon Studios
The `VOL.<N>_<theme>` naming pattern with `LP`/`NN` (Low Poly / Nanite) subfolders and `SM_*` mesh prefixes matches the Dekogon Studios "VOL." series on Fab. Verified by user against their Fab library.

- **Seller:** Dekogon Studios
- **License:** Fab Standard License
- **Required attribution:** None required under Fab Standard.

Project folders:
- `Content/Eighties/VOL4_Technology/` — likely "Props/Buildings VOL.4 — Technology" or similar Dekogon Eighties-themed VOL.4 pack
- `Content/Fences/VOL2/` — likely Dekogon Fences VOL.2
- `Content/Vehicles/VOL3_RetroCars/` — likely Dekogon Retro Cars VOL.3

### Ultra Dynamic Sky
- **Source:** https://www.fab.com/listings/84fda27a-c79f-49c9-8458-82401fb37cfb
- **Seller:** Everett Gunther
- **License:** Fab Standard License
- **Used in:** `Content/UltraDynamicSky/`
- **Required attribution:** None required under Fab Standard.

### Piano pack (PianoVol1) — Fab marketplace
- **Source:** https://www.fab.com/listings/1fa3c7ca-6f73-449e-b9c1-c94bfc2a97fc
- **Seller:** _verify in your Fab library_ — the listing page blocks scraping (HTTP 403), so the seller name couldn't be pulled automatically. The URL above is your acquired listing.
- **License:** Fab Standard License (typical — confirm the tier on the listing page)
- **Used in:** `Content/Blueprints/BP_Piano.uasset` (grand piano + stool) placed in FirstPersonMap. Only the referenced subset of `Content/PianoVol1/` is committed to git (`Mesh/SM_002_Piano001`, `Mesh/SM_002_PianoStool001`, 8 materials, 7 textures); the marketplace demo room and unused instruments remain local-only.
- **Required attribution:** None required under Fab Standard.

### Epic-included content (no attribution required, listed for audit only)
- `Content/StarterContent/` — Epic Starter Content
- `Content/FirstPerson/`, `Content/FirstPersonArms/`, `Content/FPWeapon/`, `Content/ThirdPerson/` — Epic templates
- `Content/LevelPrototyping/` — Epic prototype materials

### Sketchfab "ApertureVR ARG - Keypad"
(https://skfb.ly/opru9) by nyctomatic is licensed under Creative Commons Attribution-NonCommercial (http://creativecommons.org/licenses/by-nc/4.0/).

---

## Code / Plugins

### NodeToCode (modified)
- **Author:** protospatial
- **Source:** https://github.com/protospatial/NodeToCode
- **License:** Apache 2.0
- **Used in:** `Plugins/NodeToCode_5.4/` — Blueprint → C++ conversion during development
- **Modifications:** https://github.com/protospatial/NodeToCode/pull/14
- **Required attribution:** Apache 2.0 requires preserving the LICENSE and NOTICE files; not a user-facing credit requirement, but the LICENSE must ship with any redistribution of plugin source/binaries.

### UnrealMCP
- **Author:** kvick (Dreamatron Studios) — [@kvickart](https://x.com/kvickart)
- **Source:** https://github.com/kvick-games/UnrealMCP
- **License:** MIT
- **Used in:** `Plugins/UnrealMCP/` — editor-side AI tooling during development (not shipped at runtime)
- **Required attribution:** MIT requires preserving the copyright notice and license text in any redistribution.

---

## Fonts

_(none yet)_



