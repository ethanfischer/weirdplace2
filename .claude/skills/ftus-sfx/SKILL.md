---
name: ftus-sfx
description: Find, download, and import sound effects from the user's FreeToUseSounds "All In One Bundle" (Gumroad). Use whenever the user asks to add an SFX/sound to the game.
---

# FTUS SFX workflow

The user owns the FTUS All In One Bundle (2.4 TB). The metadata CSV is on disk;
wavs are zipped "sets" (e.g. `DESIGNED_01`) on their Gumroad library page, and
only the sets fetched so far are extracted locally.
`scripts/sfx.py` is the entry point for everything.

## Steps

1. **Search the catalog first** — before freesound, synthesis, or anything else:
   ```
   python scripts/sfx.py search <terms>        # any-term, word-boundary matched
   python scripts/sfx.py search --all <terms>  # all terms required
   ```
   Try several phrasings (e.g. "door creak", "hinge", "squeak"). Terms match on
   word boundaries, so "phone" won't hit microphone/headphone. Rows tagged
   `[LOCAL]` are already downloaded.

2. **Group candidates by set** and ask the user which sets to download
   (AskUserQuestion). Sets are large zips — never auto-download without asking:
   ```
   python scripts/sfx.py sets <terms>
   ```

3. **Fetch** the chosen sets. Downloads the 48kHz/24bit zip straight from the
   Gumroad page (no browser), archives it to the bundle's `Zips\` folder,
   extracts wavs to `Library\<CATEGORY>\<SET>\...`, then checks every CSV row of
   the set resolves to an extracted wav. Run it on an already-extracted set to
   re-check it. Run in the background — sets are GBs:
   ```
   python scripts/sfx.py fetch AMBIENCE_01 [MORE_SETS...]
   ```
   Needs `gumroad_url` in
   `C:\Users\ethan\Music\ALL IN ONE SOUND LIBRARY BUNDLE\.sfxcli.json`.

4. **Import** the picked clip into UE (editor must be running —
   `scripts/launch_editor.ps1`). Errors unless UE actually created the asset;
   only then appends the credit to CREDITS.md. In Git Bash, prefix with
   `MSYS_NO_PATHCONV=1` or `/Game/...` gets rewritten to a Windows path:
   ```
   MSYS_NO_PATHCONV=1 python scripts/sfx.py import <RecID> [--name S_MySound] [--dest /Game/Sounds]
   ```

5. Wire the SoundWave into gameplay code as requested.

## Gotchas

- The library has no isolated "dial tone" clip (checked 2026-06-18) — synthesize
  steady tones instead of slicing field recordings.
- Most clips are field recordings with background ambience, not clean isolated
  one-shots — read Description before promising a clean effect; trimming in UE
  or a DAW may be needed.
- `locate <RecID>` prints the local wav path; errors with the needed set name if
  not yet downloaded.
- The index is the **4824** CSV (`2. All In One Bundle Metadata in 4824`), which
  matches the 48/24 zips byte-for-byte in filenames (`L,R` channel tags) and
  set grouping (e.g. small categories live in one `ALL_CATEGORIES_UNDER 4GB`
  set). The other CSV ("Until 2025") describes the 192kHz zips: different
  RecIDs and sets — don't mix them. RecIDs in older CREDITS.md lines may be from it.
- The Gumroad page reuses set names across groups (2026 update packs have their
  own `METAL_02` etc., not in either CSV). `fetch` only takes zips from the
  "All In One Bundle Before 2026 In 4824" group. `Library\METAL\METAL_02` and
  `Library\FOOTSTEPS\FOOTSTEPS_02` are stale extractions of 2026-pack zips from
  before that fix — not in the index.
- Many library paths exceed MAX_PATH (260); sfx.py uses `\\?\` paths and imports
  from a short staging copy in `%TEMP%\sfx_import`.
- UE rejects >2-channel wavs (e.g. `L,R,C` files) — `import` fails loudly; pick a
  stereo/mono take.
- More background: memory file `reference_ftus_sound_library.md`.
