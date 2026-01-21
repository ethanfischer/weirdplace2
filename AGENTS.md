AGENTS guide for weirdplace2 (Unreal Engine 5.4, Windows-first)

## Purpose
- Give agentic coders fast context for build, test, and style.
- Assume worktree may be large; prefer minimal file scans.
- No Cursor or Copilot repo rules found as of this guide.

## Repository layout
- Root project file: weirdplace2.uproject
- Game code: Source/weirdplace2/*.h, *.cpp
- Build configs: Source/weirdplace2.Build.cs, Target.cs files
- Assets: Content/** (do not text-edit uasset files)
- Saved/Windows folders are generated; avoid manual edits.

## Engine and toolchain
- Target engine: Unreal Engine 5.4.
- Primary platform: Windows (Win64). Prefer Windows terminals over WSL.
- IDEs: Rider/Visual Studio; Live Coding supported for C++ body edits.
- Required SDKs: VS C++ toolset with UE dependencies; ensure `vswhere` finds them.

## Build and run commands
- Open editor (interactive): `"<UE>/Engine/Binaries/Win64/UnrealEditor.exe" "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject"`
- Build game module only: `"<UE>/Engine/Build/BatchFiles/Build.bat" weirdplace2 Win64 Development -Project="C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -WaitMutex -FromMsBuild`
- Build editor target (typical for C++ changes): `"<UE>/Engine/Build/BatchFiles/Build.bat" weirdplace2Editor Win64 Development -Project="C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -WaitMutex -FromMsBuild`
- Regenerate project files (if modules/targets change): `"<UE>/Engine/Binaries/Win64/UnrealEditor.exe" "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -projectfiles`
- Package example (adjust output paths): `"<UE>/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun -project="C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -nop4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive`
- Run headless map (quick smoke): `"<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" ThirdPersonMap -nosplash -unattended -log`
- Live Coding toggle (UE editor): Ctrl+Alt+F11 after saving .cpp edits.

## Live Coding vs full rebuild (from CLAUDE.md)
- Use Live Coding for implementation-only .cpp body changes.
- Full editor restart and rebuild required when: adding/removing UPROPERTY/UFUNCTION; changing signatures in headers; adding new classes/files; changing class inheritance; modifying *.Build.cs.
- After such changes: close UE, rebuild from Rider/VS, then reopen editor.

## Local build/git tips (from CLAUDE.md)
- Rebuild guidance to tell Ethan: Live Coding for .cpp body edits only; full restart/rebuild for UPROPERTY/UFUNCTION/signature/header/class/.Build.cs changes.
- Windows git performance: enable fsmonitor and untrackedcache (`git config core.fsmonitor true`, `git config core.untrackedcache true`) using Windows git; WSL git invalidates the cache.
- Prefer Windows git (`git.exe status` from PowerShell); WSL git slows status. Lazygit works best from Windows terminal.
- macOS UE5.4 note: edit `Engine/Config/Apple/Apple_SDK.json` MaxVersion to `26.9.0` if Xcode 26.x is in use.
- Feature docs: keep `features.md` updated before/after feature work.

## Testing and automation
- No in-repo unit tests detected; rely on Unreal Automation Tests when present.
- Run all automation tests: `"<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" -ExecCmds="Automation RunTests All; Quit" -unattended -nopause -nosplash -NullRHI`
- Run a single test by filter: replace `All` with test filter, e.g. `Automation RunTests Project.MyTest`
- Smoke PIE launch (manual verification): start UnrealEditor.exe, open desired map, use PIE.
- Enable -NullRHI for non-render test runs to speed up CI-like jobs.

## Linting and formatting
- No automated formatter configured; follow Unreal coding style shown in Source/weirdplace2.
- Keep includes minimal; prefer forward declarations in headers to reduce compile times.
- Place `GENERATED_BODY()` first inside UCLASS/USTRUCT/UINTERFACE/UGameplayTags.
- Use PascalCase for class/struct names; camelCase for local variables; Prefix bools with `b` only when consistent with surrounding code (e.g., bIsInInventoryRoom).
- Brace style: opening brace on new line after signature (project files follow default UE templates).
- Tabs appear in some files; keep existing indentation style per file (do not reformat wholesale).
- Keep UPROPERTY/UFUNCTION metadata tidy on their own line; align categories with existing patterns.

## Headers and modules
- Public includes from CoreMinimal.h first, then engine/framework headers, then local headers.
- Avoid including heavy headers (e.g., Engine.h) in headers; prefer in .cpp.
- Use forward declarations for classes referenced as pointers/refs in headers (e.g., `class UInventoryComponent;`).
- Update weirdplace2.Build.cs when adding module dependencies; this triggers a full rebuild.
- Target files: weirdplace2.Target.cs and weirdplace2Editor.Target.cs for platform tweaks.

## Memory, ownership, and Unreal specifics
- Mark UObject/UActorComponent pointers with UPROPERTY when owned/serialized or to keep from GC; use `UPROPERTY()` even for private members that must persist.
- Use `CreateDefaultSubobject` in constructors for owned components; attach appropriately.
- Avoid raw new/delete for UObject types; use UE spawn/construct helpers.
- Use `TArray`, `TMap`, `TSet` instead of STL containers to maintain reflection/GC friendliness.
- Prefer `TSubclassOf<AActor>` for Blueprint-exposed class picks.
- When loading assets at runtime, prefer `ConstructorHelpers::FObjectFinder` in constructors or `LoadClass`/`LoadObject` with safety logs (see InventoryRoomComponent).

## Gameplay patterns observed
- Inventory uses `EInventoryItem` enum with Blueprint-friendly metadata; keep new items typed and add DisplayName.
- InventoryComponent broadcasts `OnInventoryChanged`; bind with `AddDynamic` and unbind if lifetime differs.
- InventoryRoomComponent teleports player to a tagged target point and spawns display actors; refresh on inventory change.
- SpawnerActorComponent reads DataTable rows and spawns MovieBox actors across shelf/bookcase grid positions.
- TimedTeleport/TimedVisibility components tick to trigger transforms/visibility; keep timers float-accumulated.

## Logging and error handling
- Use UE_LOG with appropriate verbosity (LogTemp currently used); prefer module-specific categories if adding many logs.
- Validate pointers before use; early-return on failure (pattern in InventoryRoomComponent).
- Use `ensure`/`check` for invariant enforcement during development; avoid crashing in shipping paths unless necessary.
- For player-facing issues, favor on-screen debug via `GEngine->AddOnScreenDebugMessage` sparingly.

## Input and interaction
- Input binding is in C++: MyCharacter binds `ToggleInventory` action in SetupPlayerInputComponent; ensure DefaultInput.ini contains mapping.
- Interactable interface is NotBlueprintable; implement `InteractWithObject(AActor*, float)` in C++; keep BlueprintCallable when adding new methods.
- When adding new input actions, update project configs and expose to BP as needed.

## Blueprint interoperability
- Keep BlueprintCallable/BlueprintPure on functions meant for Blueprint graphs; BlueprintNativeEvent for overridable logic (see AddItemToInventory).
- Add metadata Category strings consistently (e.g., "Inventory" or "Inventory Room|Layout").
- Avoid changing property names/types lightly; it can break saved Blueprint assets and requires full rebuild.
- Use Editor folder paths (SetFolderPath) under WITH_EDITOR guards when organizing spawned actors.

## Testing data and assets
- DataTable lookups depend on row names; ensure CSV/JSON assets exist before spawning (SpawnerActorComponent expects DataTable set in editor).
- InventoryRoomComponent loads BP_Envelope and BP_Key at runtime if mappings list is empty; keep paths updated if assets move.
- When spawning actors, set `SpawnCollisionHandlingOverride` to AlwaysSpawn where overlap is expected.

## Error-prone areas and gotchas
- InventoryRoom teleport assumes owner is a Character for controller rotation sync; add null checks when reusing with other pawns.
- SpawnerActorComponent assumes ShelfLocations and BookcaseLocations arrays are populated and uses magic indices (ChosenOnes); guard if altering sizes.
- Timed components tick every frame; disable ticking if not needed to save perf.
- When adding new enum values, audit switch statements in Blueprints that may not handle them.

## Asset and content hygiene
- Never edit .uasset/.umap with text tools; use UE Editor.
- Avoid committing temporary files under Saved/ and Intermediate/; .gitignore should cover most.
- Keep Content folder changes small and purposeful; large binaries inflate repo.

## Git and performance (Windows note)
- Repo uses large asset set; enable fast status: `git config core.fsmonitor true` and `git config core.untrackedcache true` (run from Windows git, not WSL).
- Prefer Windows git (`git.exe`) to keep fsmonitor daemon cache valid; WSL git invalidates it and slows status checks.
- Do not run destructive git commands (no hard resets) without explicit user request.

## Documentation expectations
- After completing a feature, document it in features.md (create if missing): include feature name, description, key files/classes, and high-level behavior.
- Before changing a feature, read features.md first when available.
- Update this AGENTS.md if build/test/style conventions change.

## Code review checklist for contributors
- Did you null-check UObject pointers before dereference?
- Did you tag new UPROPERTY/UFUNCTION changes that require full rebuild?
- Did you bind/unbind delegates to avoid dangling references?
- Did you keep Blueprint metadata categories consistent?
- Did you avoid broad refactors that would break existing indentation/formatting?
- Did you add UE_LOG context for new error paths?

## Performance considerations
- Avoid per-tick heavy work unless necessary; use timers or latent actions instead of tight Tick loops.
- For spawn loops (SpawnerActorComponent), precompute values when possible and minimize string formatting.
- Use `PrimaryComponentTick.bCanEverTick = false` when ticking not required.

## Testing recommendations (when adding tests)
- Prefer Functional/Automation tests for gameplay; place under Source/<Module>/Private/Tests with IMPLEMENT_SIMPLE_AUTOMATION_TEST.
- Register tests with `IMPLEMENT_SIMPLE_AUTOMATION_TEST` and `ADD_LATENT_AUTOMATION_COMMAND` patterns.
- Use -NullRHI and -unattended in test runs to reduce flakiness on Windows builders.
- Keep test maps minimal; avoid loading heavy assets unless under test.

## Pull request hygiene for agents
- Run at least a module build before opening PR to ensure headers compile.
- Mention whether Live Coding or full rebuild is needed for your change.
- Note if Blueprint assets must be re-saved in editor after C++ changes.
- Avoid reformatting auto-generated comments; keep diffs small.

## When adding new modules/features
- Update weirdplace2.Build.cs dependencies; keep private/public split clean.
- Add new classes under Source/weirdplace2; ensure module exports use WEIRDPLACE2_API in public declarations.
- Add Target.cs tweaks for new platforms if ever needed; current focus is Win64.

## Runtime configuration tips
- Use tags (e.g., InventoryRoom) to auto-discover targets instead of hardcoding references.
- Store transforms before teleporting players; restore controller rotation as in InventoryRoomComponent.
- Use DisplayInfo mappings for spawned inventory visuals; add scale/rotation overrides per item.

## Final reminders for agents
- Prefer making targeted edits; avoid sweeping style changes.
- Mention required rebuild type (Live Coding vs full rebuild) in handoff notes.
- Keep path quoting on Windows commands; spaces in paths require quotes.
- If you add tests or build scripts, update this AGENTS.md accordingly.
