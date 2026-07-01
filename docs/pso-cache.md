# Bundled PSO (Shader Pipeline) Cache

Fixes first-run shader/PSO **compilation hitches** — the "look-around" stalls that were
traced (Unreal Insights) to cold-cache `RHICreateGraphicsPipelineState` on the render
thread (up to 1.4s) and Nanite streaming. With a bundled cache, the PSOs a playthrough
uses are **precompiled at load** instead of synchronously mid-frame. Standard on PS5.

See `memory/project_perf_streaming_hitches.md` for the full diagnosis.

## What's configured (committed)

`Config/DefaultEngine.ini`:
- `[SystemSettings] r.PSOPrecache.Resources=1` — closes the one PSO-precache gap (Components + `ProxyCreationWhenPSOReady` are already on by engine default).
- `[SystemSettings] r.ShaderPipelineCache.Enabled=1` — load + precompile the bundled cache at runtime.
- `[DevOptions.Shaders] NeedsShaderStableKeys=true` — make the cook emit `.shk` stable keys (needed to build the cache; off by default).

Artifacts:
- `Build/Windows/PipelineCaches/weirdplace2.spc` — **durable** stable cache (shader-independent; the source of truth, keep in source control).
- `Content/PipelineCaches/Windows/weirdplace2_PCD3D_SM6.stable.upipelinecache` — the **built** binary cache that ships (642 PSOs). Build-specific — regenerate when shaders change.

## Verified working
Cooked Win64 Development run logs:
`Opened FPipelineCacheFile ...weirdplace2_PCD3D_SM6.stable.upipelinecache with 642 entries`
→ `enqueued 642 tasks for precompile (642 eligible, 0 had missing shaders)` → completed at load.

## Regenerating the cache (after shader-affecting changes)

The cooker's own pipeline-cache step mangles a pre-built binary cache (drops it to ~36
PSOs) because it re-maps against freshly-cooked shaders. **The reliable flow is: cook
first, build the cache from the cook's own keys, then RE-STAGE — never full-cook after
placing the cache.**

1. **Cook** (full) — generates shaders + `.shk` stable keys:
   ```
   RunUAT BuildCookRun -project=weirdplace2.uproject -platform=Win64 -clientconfig=Development \
     -cook -allmaps -build -stage -pak -archive -archivedirectory=Packaged/Windows-Dev -unattended
   ```
   (Close the editor first — a running editor locks the DLL and the build fails.)

2. **Record** the PSOs a playthrough uses (reuses the runtime perf-walk):
   ```
   Packaged/Windows-Dev/weirdplace2.exe -PerfWalk -logpso -windowed -resx=1280 -resy=720 -nosplash
   ```
   → `Packaged/Windows-Dev/weirdplace2/Saved/CollectedPSOs/*.rec.upipelinecache`
   (For real coverage, also play through anything PerfWalk doesn't touch.)

3. **Expand** recorded PSOs + stable keys → durable `.spc` (use ABSOLUTE paths):
   ```
   UnrealEditor-Cmd weirdplace2.uproject -run=ShaderPipelineCacheTools Expand \
     <abs>/...rec.upipelinecache \
     <abs>/Saved/Cooked/Windows/weirdplace2/Metadata/PipelineCaches/ShaderStableInfo-Global-PCD3D_SM6.shk \
     <abs>/Saved/Cooked/Windows/weirdplace2/Metadata/PipelineCaches/ShaderStableInfo-weirdplace2-PCD3D_SM6.shk \
     <abs>/Build/Windows/PipelineCaches/weirdplace2.spc -DisablePlugins=Fab -unattended
   ```
   (Use `.spc`, not the deprecated `.stablepc.csv`.)

4. **Build** the binary cache from the `.spc` + THIS cook's `.shk`, named exactly what the
   runtime opens, into project Content:
   ```
   UnrealEditor-Cmd weirdplace2.uproject -run=ShaderPipelineCacheTools Build \
     <abs>/Build/Windows/PipelineCaches/weirdplace2.spc \
     <abs>/.../ShaderStableInfo-Global-PCD3D_SM6.shk <abs>/.../ShaderStableInfo-weirdplace2-PCD3D_SM6.shk \
     <abs>/Content/PipelineCaches/Windows/weirdplace2_PCD3D_SM6.stable.upipelinecache -DisablePlugins=Fab -unattended
   ```
   Expect `Wrote NNN binary PSOs`.

5. **Re-stage only** (NOT a full cook) so shaders don't change and the cache maps 100%:
   ```
   RunUAT BuildCookRun -project=weirdplace2.uproject -platform=Win64 -clientconfig=Development \
     -skipbuild -skipcook -stage -pak -archive -archivedirectory=Packaged/Windows-Dev -unattended
   ```
   If a full cook re-mangles the cache to ~36 entries, also copy the built `.upipelinecache`
   over `Saved/Cooked/Windows/weirdplace2/Content/PipelineCaches/Windows/` and
   `.../Metadata/PipelineCaches/` before re-staging.

6. **Verify**: run the exe and grep the log for `Opened FPipelineCacheFile ... with N entries`
   and `0 had missing shaders`.

## Notes
- The recorded set only covers what was exercised. Expand coverage by recording a broader
  play session, not just `PerfWalk`.
- PS5 ships a pipeline cache by default; this same `.spc` flow applies per platform (cook
  for that platform, use that platform's `.shk`).
- Residual non-PSO hitches (Nanite `CreateCommittedResource` GPU allocation on the 8GB dev
  card) are separate — tune `r.Nanite.Streaming.StreamingPoolSize`; largely gone on PS5/16GB.
