# Cutting a track into intro + seamless endless loop

Used for `Passing.mp3` (2026-08-27). Scripts: `scripts/local/music_loop/`
(edit the constants at the top for a new track). Requires
`pip install librosa soundfile matplotlib`.

## Result for Passing

- Source: `Content/Sounds/Passing.mp3` (4:03 ambient, beatless)
- `Passing_Intro.wav` — 0:00–97.34s
- `Passing_Loop.wav` — 97.34s–150.49s (53.15s), loops endlessly; the quiet outro is dropped
- Outputs land in `Saved/PassingLoop/` (gitignored) along with a `preview.html`
  Web Audio auditioner (`python -m http.server 8765 -d Saved/PassingLoop`).

## Method (works on beatless/ambient material — no bar grid needed)

1. **`analyze_passing.py`** — waveform/RMS/spectrogram overview to see the
   structure and pick the search region (avoid the fade-out).
2. **`find_loop.py`** — self-similarity search: MFCC (timbre) + chroma
   (harmony) + RMS (level) features, compare the ~3s of audio *following*
   every candidate loop-start against the ~3s following every candidate
   loop-end (min 40s apart). Low distance = the jump end→start will land in
   matching texture. Prints the top 10 (start, end) pairs.
3. **`cut_loop.py`** — refine the loop-end at sample level via
   cross-correlation (±0.75s search), then cut. **The seam trick:** the final
   1s of the loop is equal-power-crossfaded into the 1s of audio that
   *originally preceded* the loop start — so at the jump, the waveform flows
   into the loop start exactly as it did in the original recording. Exports
   intro + loop + a stress-test WAV (intro + loop ×3) for auditioning.

Key rules regardless of tool:
- Never loop an MP3 (encoder padding = gap/click). Export WAV, import WAV.
- Verify by ear with the stress test / preview.html; the seam repeats every
  loop-length seconds.
