---
name: ftus-sfx
description: Find, download, and import sound effects from the user's FreeToUseSounds "All In One Bundle" (Gumroad). Use whenever the user asks to add an SFX/sound to the game.
---

# FTUS SFX workflow

The user owns the FTUS All In One Bundle (2.4 TB). Only the metadata CSV is on
disk; wavs are zipped "sets" (e.g. `AMBIENCE_59`) on their Gumroad library page.
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

3. **Fetch** the chosen sets. This opens the Gumroad page in the user's browser;
   *they must click the set's download link* — tell them so. The CLI then watches
   `~/Downloads`, archives the zip to the bundle's `Zips\` folder, and extracts
   wavs to `Library\<CATEGORY>\<SET>\...`:
   ```
   python scripts/sfx.py fetch AMBIENCE_59 [MORE_SETS...]
   ```
   First run needs `gumroad_url` in
   `C:\Users\ethan\Music\ALL IN ONE SOUND LIBRARY BUNDLE\.sfxcli.json`.

4. **Import** the picked clip into UE (editor must be running —
   `scripts/launch_editor.ps1`). This also appends the credit to CREDITS.md
   automatically:
   ```
   python scripts/sfx.py import <RecID> [--name S_MySound] [--dest /Game/Sounds]
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
- More background: memory file `reference_ftus_sound_library.md`.
