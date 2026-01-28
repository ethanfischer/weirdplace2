Feature: VHS Cover Pipeline and Texture Optimization
- Purpose: randomize VHS box covers in-world while keeping memory use manageable.
- Key files: Content/Python/create_vhs_material_instances.py; Content/Python/optimize_vhs_textures.py; Content/CSVs/vhs_covers.csv; Content/CreatedMaterials/VHSCoverMaterials/*; Source/weirdplace2/MovieBox.cpp; Source/weirdplace2/SpawnerActorComponent.cpp.
- Behavior: Spawner names spawned boxes with a DataTable row name plus index (e.g., TITLE_5); MovieBox strips the numeric suffix and loads MI_VHSCover_<TITLE> from /Game/CreatedMaterials/VHSCoverMaterials; MI generator builds material instances for each Texture2D under /Game/VHSCovers; CSV lists covers for data table import.
- Optimization: optimize_vhs_textures.py now caps MaxTextureSize at 512, enables streaming, leaves virtual texture streaming off by default, keeps sRGB and default compression, nudges mip gen off NoMipmaps when present. Run in UE Output Log: py "Content/Python/optimize_vhs_textures.py".
- Usage notes: run optimizer after importing new cover textures and before regenerating MIs; MI generator imports the optimizer helper so new runs enforce the 512 cap. No C++ rebuild needed for script or content-only changes.

Feature: MovieBox interaction/inspection
- Purpose: let the player inspect/rotate a MovieBox, reveal the back, and collect a hidden item into inventory.
- Key files: Source/weirdplace2/MovieBox.cpp (InteractWithObject, RotateInspectedActor, CollectInspectedSubitem, StopInspection); MovieBox.h; Interactable.h; input mappings in project settings/DefaultInput.ini.
- Input flow: Interact action (E) calls InteractWithObject to start inspection; rotation uses axes "Turn Right / Left Mouse" and "Turn Right / Left Gamepad"; collect uses action "Collect Inspected Subitem" (bind to E or your chosen key); exit uses action "Exit Interaction" (Q by default).
- Behavior: On interact, the box is moved in front of the camera, player look/move is disabled, rotation axes are bound. While rotating, when the box back faces the camera (dot > 0.9), the widget shows and the collect action is bound; turning away hides it and unbinds. Collect hides the mesh, marks collected, calls AddItemToInventory(EInventoryItem::InventoryItem1), logs the cover name, and auto-exits inspection. Interact (E) now also exits while inspecting (in addition to the Exit Interaction action). Exit restores the original transform, re-enables input, and clears bindings.
- Inventory display: Collected movie covers are recorded per-cover and shown in the inventory room as MovieBoxDisplayActor instances with the matching `MI_VHSCover_<Cover>` material instead of envelopes (fallback to envelopes if no covers exist or class unset).
- Notes: ensure input mappings exist for the three actions/axes; no BP override needed—BP_MovieBox just calls the C++ interface implementation. No rebuild needed unless you change headers or UPROPERTY/UFUNCTIONs.

Feature: Inventory Room Item Name Display
- Purpose: Show the name of the VHS cover the player is looking at as 3D text below the inventory grid.
- Key files: Source/weirdplace2/InventoryRoomComponent.h; Source/weirdplace2/InventoryRoomComponent.cpp; Content/Materials/M_UnlitText (unlit text material).
- Behavior: When in the inventory room, a UTextRenderComponent displays below the grid. Each tick, a bounding-box intersection test checks which spawned display actor the player's camera is looking at. If the look ray hits an actor's bounds, the corresponding cover name is shown. Text clears when not looking at any item.
- Configuration (on InventoryRoomComponent in Details panel):
  - TextWorldSize (default 12): Size of the 3D text.
  - TextVerticalOffset (default 60): How far below the first grid row the text appears.
  - TextColor (default White): Color of the text.
  - TextMaterial: Assign M_UnlitText (or any unlit material) to prevent shadows affecting the text.
- Implementation notes: Cover names are stored in a TMap<AActor*, FString> (ActorCoverNames) since BP_MovieBox doesn't inherit from AMovieBoxDisplayActor. The text material must sample a TextureSampleParameter2D named "FontTexture" to render glyphs correctly.
- Python helper: Content/Python/create_unlit_text_material.py creates M_UnlitText programmatically. Run headless: `UnrealEditor-Cmd.exe <project> -run=pythonscript -script="Content/Python/create_unlit_text_material.py"`.

Feature: Inventory Room Background Capture
- Purpose: Capture the player's view before teleporting to inventory room and display it as a blurred background on the wall, creating a seamless transition effect.
- Key files: Source/weirdplace2/InventoryRoomComponent.h; Source/weirdplace2/InventoryRoomComponent.cpp; Content/Materials/M_BlurredBackground; Content/Python/create_blur_background_material.py.
- Behavior: When TeleportToInventoryRoom() is called, CaptureAndApplyBackground() runs first. It creates a USceneCaptureComponent2D at the player's camera position, captures the scene to a UTextureRenderTarget2D, then creates a dynamic material instance from BackgroundBlurMaterial with the render target as a texture parameter. This is applied to BackgroundWallActor. On exit, RestoreWallMaterial() reverts to the original material.
- Configuration (on InventoryRoomComponent in Details panel):
  - BackgroundWallActor: The wall mesh in the inventory room to apply the captured background to.
  - BackgroundBlurMaterial: Assign M_BlurredBackground (or custom material with "BackgroundTexture" parameter).
  - CaptureResolution (default 512): Render target size. Lower values (128-256) create more blur due to upscaling.
  - BlurStrength (default 5.0): Passed to material as scalar parameter (material must support it).
- Implementation notes: The blur effect comes primarily from using a low-resolution capture that gets stretched to wall size. For proper Gaussian blur, create a custom material with multiple offset texture samples. SceneCaptureComponent2D uses SCS_FinalColorLDR capture source for post-processed output.
- Python helper: Content/Python/create_blur_background_material.py creates M_BlurredBackground. Run headless: `UnrealEditor-Cmd.exe <project> -run=pythonscript -script="Content/Python/create_blur_background_material.py"`.
