Feature: VHS Cover Pipeline and Texture Optimization
- Purpose: randomize VHS box covers in-world while keeping memory use manageable.
- Key files: Content/Python/create_vhs_material_instances.py; Content/Python/optimize_vhs_textures.py; Content/CSVs/vhs_covers.csv; Content/CreatedMaterials/VHSCoverMaterials/*; Source/weirdplace2/MovieBox.cpp; Source/weirdplace2/SpawnerActorComponent.cpp.
- Behavior: Spawner names spawned boxes with a DataTable row name plus index (e.g., TITLE_5); MovieBox strips the numeric suffix and loads MI_VHSCover_<TITLE> from /Game/CreatedMaterials/VHSCoverMaterials; MI generator builds material instances for each Texture2D under /Game/VHSCovers; CSV lists covers for data table import.
- Optimization: optimize_vhs_textures.py now caps MaxTextureSize at 512, enables streaming, leaves virtual texture streaming off by default, keeps sRGB and default compression, nudges mip gen off NoMipmaps when present. Run in UE Output Log: py "Content/Python/optimize_vhs_textures.py".
- Usage notes: run optimizer after importing new cover textures and before regenerating MIs; MI generator imports the optimizer helper so new runs enforce the 512 cap. No C++ rebuild needed for script or content-only changes.

Feature: MovieBox interaction/inspection
- Purpose: let the player inspect/rotate a MovieBox, reveal the back, and collect a hidden item into inventory.
- Key files: Source/weirdplace2/MovieBox.cpp (Interact_Implementation, RotateInspectedActor, CollectInspectedSubitem, StopInspection); MovieBox.h; Interactable.h; input mappings in project settings/DefaultInput.ini.
- Input flow: Interact action (E) calls Interact_Implementation to start inspection; rotation uses axes "Turn Right / Left Mouse" and "Turn Right / Left Gamepad"; collect uses action "Collect Inspected Subitem" (bind to E or your chosen key); exit uses action "Exit Interaction" (Q by default).
- Behavior: On interact, the box is moved in front of the camera, player look/move is disabled, rotation axes are bound. While rotating, when the box back faces the camera (dot > 0.9), the widget shows and the collect action is bound; turning away hides it and unbinds. Collect hides the mesh, marks collected, calls AddItemToInventoryWithMesh() with the cover FName and mesh data, and auto-exits inspection. Interact (E) now also exits while inspecting (in addition to the Exit Interaction action). Exit restores the original transform, re-enables input, and clears bindings.
- Notes: ensure input mappings exist for the three actions/axes; no BP override needed—BP_MovieBox just calls the C++ interface implementation. No rebuild needed unless you change headers or UPROPERTY/UFUNCTIONs.

Feature: Inventory Room Item Name Display (LEGACY - see Diegetic Inventory UI)
- Purpose: Show the name of the VHS cover the player is looking at as 3D text below the inventory grid.
- Key files: Source/weirdplace2/InventoryRoomComponent.h; Source/weirdplace2/InventoryRoomComponent.cpp; Content/Materials/M_UnlitText (unlit text material).
- Note: InventoryRoomComponent is DEPRECATED in favor of InventoryUIComponent.
- Behavior: When in the inventory room, a UTextRenderComponent displays below the grid. Each tick, a bounding-box intersection test checks which spawned display actor the player's camera is looking at. If the look ray hits an actor's bounds, the corresponding cover name is shown. Text clears when not looking at any item.
- Configuration (on InventoryRoomComponent in Details panel):
  - TextWorldSize (default 12): Size of the 3D text.
  - TextVerticalOffset (default 60): How far below the first grid row the text appears.
  - TextColor (default White): Color of the text.
  - TextMaterial: Assign M_UnlitText (or any unlit material) to prevent shadows affecting the text.
- Implementation notes: Cover names are stored in a TMap<AActor*, FString> (ActorCoverNames) since BP_MovieBox doesn't inherit from AMovieBoxDisplayActor. The text material must sample a TextureSampleParameter2D named "FontTexture" to render glyphs correctly.
- Python helper: Content/Python/create_unlit_text_material.py creates M_UnlitText programmatically. Run headless: `UnrealEditor-Cmd.exe <project> -run=pythonscript -script="Content/Python/create_unlit_text_material.py"`.

Feature: Inventory Room Background Capture (LEGACY - see Diegetic Inventory UI)
- Purpose: Capture the player's view before teleporting to inventory room and display it as a blurred background on the wall, creating a seamless transition effect.
- Key files: Source/weirdplace2/InventoryRoomComponent.h; Source/weirdplace2/InventoryRoomComponent.cpp; Content/Materials/M_BlurredBackground; Content/Python/create_blur_background_material.py.
- Note: InventoryRoomComponent is DEPRECATED in favor of InventoryUIComponent.
- Behavior: When TeleportToInventoryRoom() is called, CaptureAndApplyBackground() runs first. It reads pixels directly from the viewport via ReadPixels(), applies a CPU-side Gaussian blur, creates a UTexture2D, then creates a dynamic material instance from BackgroundBlurMaterial with the texture as a parameter. This is applied to BackgroundWallActor. On exit, RestoreWallMaterial() reverts to the original material.
- Configuration (on InventoryRoomComponent in Details panel):
  - BackgroundWallActor: The wall mesh in the inventory room to apply the captured background to.
  - BackgroundBlurMaterial: Assign M_BlurredBackground (or custom material with "BackgroundTexture" parameter).
  - CaptureResolution (default 512): Render target size. Lower values (128-256) create more blur due to upscaling.
  - BlurStrength (default 5.0): Passed to material as scalar parameter (material must support it).
- Implementation notes: The blur effect uses CPU-side Gaussian blur on captured viewport pixels, then uploads to a transient texture.
- Python helper: Content/Python/create_blur_background_material.py creates M_BlurredBackground. Run headless: `UnrealEditor-Cmd.exe <project> -run=pythonscript -script="Content/Python/create_blur_background_material.py"`.

Feature: Diegetic Inventory UI
- Purpose: Display inventory as a floating 3D grid UI in front of the player (VR-friendly, no screen-space UI).
- Key files: Source/weirdplace2/InventoryUIActor.h/.cpp; Source/weirdplace2/InventoryUIComponent.h/.cpp; Content/Materials/M_SolidColor; Content/Materials/M_VHSCoverFront.
- Behavior: Press Tab to toggle inventory. A grid of thumbnail slots appears in front of the camera. Look at slots to select (reticle-based selection). Press E to confirm selection, setting the active item. Press Q to close.
- Visual elements: Empty slots shown as dark rectangles; collected items show front-face thumbnails; yellow pulsing highlight on hovered slot; green border on active (confirmed) item; item name text at bottom.
- Thumbnail display: Uses M_VHSCoverFront material which samples only the front face portion of VHS cover textures (U: 0.55-1.0 of texture) with 90° UV rotation for correct orientation.
- Configuration (on InventoryUIComponent): GridColumns/GridRows (default 4x3); InventoryDistance; VerticalOffset; AnimationDuration.
- Python helper: Content/Python/create_vhs_front_material.py creates M_VHSCoverFront. Run: `py "C:/Users/ethan/repos/weirdplace2/Content/Python/create_vhs_front_material.py"`.

Feature: Active Item Selection
- Purpose: Allow player to select an item from inventory to be their "active" item.
- Key files: Source/weirdplace2/Inventory.h/.cpp (FInventoryItemData struct, SetActiveItem, GetActiveItem, OnActiveItemChanged delegate).
- Behavior: When player confirms selection in inventory UI (E key), that item becomes the active item. The inventory stores visual data (mesh, materials, scale, rotation) captured when items are collected via AddItemToInventoryWithMesh().
- Data flow: MovieBox::CollectInspectedSubitem() calls MyCharacter::AddItemToInventoryWithMesh() which captures FInventoryItemData from the mesh component and stores it in InventoryComponent's ItemDataMap.

Feature: Held Item Display
- Purpose: Show the active/selected item in the player's view (as if held in hand).
- Key files: Source/weirdplace2/HeldItemComponent.h/.cpp; Source/weirdplace2/MyCharacter.h/.cpp.
- Behavior: When an active item is set and inventory closes, the held item mesh appears attached to the camera. Uses stored FInventoryItemData (mesh, materials, scale) from inventory. Hidden while inventory is open.
- Configuration (on HeldItemComponent): HeldItemOffset (default 30, 25, 40); HeldItemRotation (default -45° pitch).
- VR note: Currently attaches to camera; TODO: attach to motion controller hand in VR mode.
