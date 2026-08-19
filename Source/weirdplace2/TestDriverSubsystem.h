#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "PayPhone.h"
#include "Subsystems/WorldSubsystem.h"
enum class EPlayerActivityState : uint8;
#include "TestDriverSubsystem.generated.h"

class AFirstPersonCharacter;
class AInspectablePickup;
class AMovieBox;
class APropActor;
class AHudson;
class ARick;
class ASeneca;
class ATestWaypoint;
class UInputAction;
class UInventoryComponent;
class UInventoryUIComponent;
class UKeypadUIComponent;

// Verb layer for E2E tests. Provides positioning, camera aiming, input
// simulation, and state queries. All gameplay actions go through the real
// input pipeline via APlayerController::InputKey — we never call gameplay
// functions directly.
UCLASS()
class WEIRDPLACE2_API UTestDriverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Character access ---

	AFirstPersonCharacter* GetPlayer() const;
	bool IsPlayerReady() const;

	// --- Movement / look ---

	bool TeleportPlayerToWaypoint(FName WaypointTag);

	// Teleport the player near an actor (Distance units away, facing it).
	// Avoids the need for per-actor waypoints.
	bool TeleportNearActor(AActor* Target, float Distance = 200.f);

	// Teleports the player capsule onto GroundPoint (capsule half-height added).
	// For screenshot vantages that no waypoint or actor-relative teleport gives.
	bool TeleportToWorldPoint(const FVector& GroundPoint);

	bool LookAt(AActor* Target);
	bool LookAtActorByLabel(const FString& Label);
	// Aim the camera at the world-space location of a named scene component on
	// an actor found by editor label. Used to target sub-features like
	// BP_OutsideBathroomDoor's "KeyLockSocket".
	bool LookAtActorComponentByName(const FString& ActorLabel, const FString& ComponentName);
	// Same, with the actor already in hand. Use when the actor's full bounds
	// center is skewed by child meshes (e.g. the blank tape's Tape child).
	bool LookAtComponentByName(AActor* Actor, const FString& ComponentName);
	USceneComponent* FindComponentOnActorByName(AActor* Actor, const FString& ComponentName) const;

	// Aim the camera at an exact world position.
	bool LookAtWorldPoint(const FVector& Point);

	// The point LookAt() aims at for a given actor: the Face mesh bounds origin
	// for MetaHumans (their root sits at Z=0), else the full bounds center.
	FVector GetAimPointForActor(AActor* Target) const;

	// --- Photo-booth staging (visual-inspection diagnostics) ---

	// Teleport the actor found by editor label onto the ATestWaypoint with the
	// given tag, recording its original transform for UnstageActor. Fails (and
	// logs Error) if actor or waypoint is missing.
	bool StageActorAtWaypoint(const FString& Label, FName WaypointTag);

	// Restore a previously staged actor to its recorded transform.
	bool UnstageActor(const FString& Label);

	// Set by FTD_TeleportNearBlankTape: a verified-hittable point just inside
	// the blank tape's collision surface. FTD_LookAtBlankTape aims here —
	// derived centers (actor bounds, envelope bounds) miss the Memphis mesh's
	// collision at some shelf-slot angles.
	FVector BlankTapeAimPoint = FVector::ZeroVector;
	bool LookAtSeneca();
	bool LookAtRick();
	bool LookAtTelephone();
	bool LookAtKeyActor();

	AActor*    FindActorByLabel(const FString& Label) const;
	ASeneca*   FindSeneca() const;
	APayPhone* FindPayPhone() const;
	ARick*     FindRick() const;
	AHudson*   FindHudson() const;
	bool       LookAtHudson();

	// --- Story-flag test helpers ---

	// Set/clear a story flag by name ("KeyBroke", "TornadoWarningDisplayed",
	// "SeenTornadoWarning"). No-op + logs if the name doesn't resolve.
	void SetStoryFlag(FName FlagName, bool bValue);

	// True if the named story flag is set. False if unset OR the name/subsystem
	// is missing (the meaningful red-phase failure).
	bool IsStoryFlagSet(FName FlagName) const;

	// Invoke the store-entry handler directly (same path the TriggerBox_Inside
	// overlap drives) without physically moving the player through the trigger.
	void TriggerStoreEntry();

	// True if the ACRTTV found by editor label has switched to its tornado-warning
	// screen. False (and logs) if no such TV exists.
	bool IsTvShowingWarning(const FString& Label) const;

	// True if the ACRTTV found by editor label is playing its looping tornado-alert
	// siren. False (and logs) if no such TV exists.
	bool IsTvWarningAudioPlaying(const FString& Label) const;

	// True if the AAmbientSound found by editor label has a playing audio
	// component. False (and logs) if no such ambient-sound actor exists.
	bool IsAmbientSoundPlaying(const FString& Label) const;

	// Max intensity across all ULightComponents on the actor found by editor label
	// (the storm-dim test compares this before/after). -1 (and logs) if no such
	// actor or it has no light component.
	float GetActorMaxLightIntensity(const FString& Label) const;

	// Reads the player's pea-soup fog state (roll-in amount 0..1 + current settled
	// fog distance in cm). False if the player has no UStormFogComponent.
	bool GetStormFogState(float& OutAmount, float& OutDistance) const;

	// --- Pay-phone (items 2/5) test helpers ---

	// True if the actor found by editor label has a visible root component.
	bool IsActorVisibleByLabel(const FString& Label) const;

	// True while the pay-phone's static/voice bed is playing.
	bool IsPayPhoneAudioPlaying() const;

	// Whether the pay-phone would accept an interact right now (gated; blocked
	// while off the hook).
	bool CanPayPhoneInteract() const;

	// True while the pay-phone's dialtone loop is playing.
	bool IsPayPhoneDialtonePlaying() const;

	// Trigger the pay-phone pickup directly (5.7 simulated-input gotcha — drive
	// via the subsystem, not a raw key).
	void TriggerPayPhonePickup();

	// Hang up the pay-phone directly (mirror of TriggerPayPhoneHangUp).
	void TriggerPayPhoneHangUp();

	// Mark the spoken code as already heard, so the next pickup is a mundane
	// "dialtone only" call (persistent looping dialtone, no first-call cut/code).
	void MarkPayPhoneCodeSpoken();

	// --- Bathroom-door (item 1) test helpers ---

	// Number of times the OutsideBathroomDoor has played its locked-rattle
	// branch. -1 (and logs) if no such door exists. The re-entrancy guard test
	// asserts this stays 0 across a mid-key-break re-entrant interact.
	int32 GetBathroomDoorLockedSoundCount() const;

	// True while the OutsideBathroomDoor's animated key carries the glow overlay
	// (during the insert sequence). False (and logs) if no such door exists.
	bool GetBathroomDoorAnimKeyGlowActive() const;

	// True if the level's AInspectablePickup (e.g. the dropped broken key) has
	// the self-illumination glow overlay. False (and logs) if none exists.
	bool GetInspectablePickupGlowActive() const;

	// --- Seneca test helpers ---

	// Skip the 60-second SmokingAppearDelay so the E2E test doesn't have to wait.
	void FastForwardSenecaSmoking();

	// Joins Seneca's effective Smoking-state lines (BuildEffectiveDialogueLines)
	// into a single string. The shelter tip is included iff SeenTornadoWarning is
	// set, so the test can assert presence/absence by flag. False if no Seneca.
	bool GetSenecaSmokingLinesJoined(FString& Out) const;

	// Jump Seneca straight into the Smoking beat (CurrentState=Smoking + teleport
	// to the smoking spot + anim) so the shelter line can be screenshotted without
	// replaying the whole quest.
	void ForceSenecaToSmoking();

	// True once Seneca has been re-teleported out of the hidden "below world"
	// position back to the smoking spot (i.e., Z > -50000).
	bool HasSenecaAppearedAtSmokingPos() const;

	// --- Input simulation ---

	// Routes a key event through APlayerController::InputKey, firing both
	// Enhanced Input actions and legacy BindAction bindings.
	void SimulateKeyPress(FKey Key);
	void SimulateKeyRelease(FKey Key);

	// Injects a MouseX axis delta through the input pipeline.
	void SimulateMouseX(float Delta);

	// Presses the "put back" binding that exits item inspection (the legacy
	// "Exit Interaction" action): keyboard Q, or gamepad B when the player's
	// last input was a gamepad.
	void SimulatePutBack();

	// Enhanced Input injection. APlayerController::InputKey only fires legacy
	// BindAction bindings — Enhanced Input actions need their own injection
	// path. These call UEnhancedInputLocalPlayerSubsystem::InjectInputForAction
	// directly so HandleInteractTriggered/HandleShowInventory etc. fire.
	void InjectInputAction(UInputAction* Action, bool bPressed);
	void SimulateInteractPress();
	void SimulateInteractRelease();
	void SimulateInventoryPress();
	void SimulateInventoryRelease();
	void SimulateSettingsPress();
	void SimulateSettingsRelease();

	// --- Inventory (queries only — open/close/confirm go through input) ---

	bool IsInventoryFullyOpen() const;
	bool IsInventoryFullyClosed() const;

	// Sets the inventory cursor to a slot index (equivalent to aiming the
	// camera at the right spot). Confirmation still goes through E key input.
	bool SetSelectedSlot(int32 Index);

	// Inventory UI selection state (absolute item index + horizontal scroll window).
	int32 GetSelectedSlot() const;
	int32 GetScrollOffset() const;
	int32 GetVisibleColumns() const;

	// --- Keypad (code-entry on locked doors) ---

	bool IsKeypadFullyOpen() const;

	// Directly fire a keypad door's Interact (pops the code keypad) by actor label,
	// bypassing the simulated interact key — which 5.7 intermittently swallows at
	// fast pacing in headed runs, leaving the keypad closed. Same rationale as
	// EnterKeypadCode below. Returns false if no ADoor has that label.
	bool TriggerKeypadDoorOpen(const FString& DoorLabel);

	// Cumulative count of wrong-code buzzes since the keypad component was created.
	// Rises WrongCodeClearDelay after each rejected submit (see ClearWrongEntry).
	int32 GetKeypadDenySoundCount() const;

	// Enter a full numeric code (digits 1-9) on the open keypad: selects each
	// digit's cell and presses it directly (bypasses Enhanced Input, which 5.7
	// double-fires/consumes for simulated presses). Submits on the last digit.
	bool EnterKeypadCode(const FString& Code);

	// --- Movie helpers ---

	// Returns the next uncollected MovieBox in the level, or nullptr.
	// Tracks already-collected boxes internally so consecutive calls return
	// different movies. Does NOT collect or interact — just finds the target.
	// Also stores the result internally so MarkLastFoundMovieCollected can
	// mark it without needing to pass the pointer between latent commands.
	AMovieBox* FindNextUncollectedMovie();

	// Marks the movie most recently returned by FindNextUncollectedMovie as
	// collected, so subsequent calls skip it.
	void MarkLastFoundMovieCollected();

	// Returns the BP_BlankVHS-class spawned blank tape (the one with
	// bExemptFromMovieLimit==true), or nullptr if none exists.
	AMovieBox* FindBlankTape() const;

	// Test-only: invoke CollectInspectedMovie directly on whichever MovieBox is
	// currently in inspection. UE 5.7 Enhanced Input intermittently consumes
	// the legacy "Collect Inspected Movie" ActionMapping when E is fed via
	// simulated input, so tests bypass the input layer here. Returns true if a
	// MovieBox was found and collect was called.
	bool TriggerCollectInspectedMovie();

	// --- Inspectable pickup helpers ---

	// Returns the first AInspectablePickup in the level (or nullptr). The
	// key-break sequence spawns one of these on the ground instead of
	// auto-adding the broken key to inventory; tests poll this to know the
	// pickup has appeared.
	AInspectablePickup* FindInspectablePickup() const;

	// Test-only: invoke CollectInspectedItem directly on whichever
	// AInspectablePickup is currently in inspection. Mirrors
	// TriggerCollectInspectedMovie — the legacy "Collect Inspected Movie"
	// binding is unreliable under simulated input in 5.7. Returns true if a
	// pickup in inspection was found and collect was called.
	bool TriggerCollectInspectedPickup();

	// --- State queries ---

	EPlayerActivityState GetActivityState() const;
	bool IsInSimpleDialogue() const;
	bool IsInAnyDialogue() const;
	bool HasItem(FName ItemId) const;
	int32 GetInventoryCount() const;

	// Returns the ItemID at inventory slot SlotIndex, NAME_None if empty or out
	// of range. Slots are sparse (OoT-style), so a given slot keeps its item.
	FName GetInventoryItemAt(int32 SlotIndex) const;

	// Fires a single bladder-urgency vignette pulse on the player (no timer
	// scheduling) — for screenshotting the pulse visual on demand.
	bool TriggerBladderPulse();

	// Sets the level's ExponentialHeightFog component visibility. Two calls
	// from consecutive latent commands make a genuine two-frame off/on cycle
	// (same-frame toggles coalesce to a no-op). For the first-play fog probes.
	bool SetHeightFogVisible(bool bVisible);

	// Finds a named scene component on the actor with the given editor label
	// and reports its visibility. Returns false if the actor or component
	// doesn't exist — which is also the meaningful missing-feature failure.
	bool GetNamedComponentVisible(const FString& ActorLabel, const FString& ComponentName, bool& bOutVisible) const;

	// Reads the depth-of-field state off the player's first-person camera:
	// whether the inspection-blur DoF overrides are active, and the current
	// Fstop / focal distance. Returns false if no player camera exists.
	bool GetCameraDofState(bool& bOutOverrideActive, float& OutFstop, float& OutFocalDistance) const;

	// Reads the in-widget dialogue text-backing state off the labeled actor's
	// dialogue widget (any IDialogueWidgetProvider): whether the Text block is
	// wrapped in the backing plate, and whether the dialogue widget is open.
	// Returns false if the actor isn't a provider or has no widget yet.
	bool GetDialogueBackingState(const FString& ActorLabel, bool& bOutHasBacking, bool& bOutDialogueOpen) const;

	// Reads the gaze-reward hum state off the level's actor tagged "GazeReward"
	// (its first UAudioComponent). Returns false if no such actor/component
	// exists — which is also the meaningful red-phase failure.
	bool GetGazeHumState(float& OutVolume, bool& bOutPlaying) const;

	// Reads the gaze-reward dwell timer off the level's UGazeRewardComponent.
	// For the GazeRewardReset test (accumulates then resets on look-away).
	// Returns false if no component exists.
	bool GetGazeRewardSeconds(float& OutSeconds) const;

	// Reads the gaze-reward screen-effect blendable weight (0..1) off the
	// level's UGazeRewardComponent. Returns false if no component exists.
	bool GetGazeEffectWeight(float& OutWeight) const;

	// Reads the player camera's current FOV (driven by the gaze FOV-zoom).
	// Returns false if no gaze component exists.
	bool GetGazeCameraFOV(float& OutFOV) const;

	// Reads the blank-VHS chord gaze state off the level's USpawnerActorComponent
	// (the last camera-forward trace it ran). For the gaze-sweep diagnostic.
	// Returns false if no spawner component exists.
	bool GetBlankVhsGazeState(bool& bOutHasChosen, bool& bOutLooking, bool& bOutHadHit,
		FString& OutHitActor, FString& OutHitComponent, float& OutHitDistance,
		FVector& OutImpactPoint, float& OutVolume) const;

	// Reads the speaker plate and full body line off the dialogue widget the
	// player is currently in. Returns false if no dialogue widget is active.
	bool GetDisplayedDialogue(FString& OutSpeaker, FString& OutBody) const;

	// Reads the put-back prompt off the MovieBox currently in inspection:
	// its text, visibility, and how squarely it faces the camera (dot of the
	// text forward vector against the to-camera direction). Returns false if
	// nothing is inspected or the inspected box has no 'PutBackPromptText'.
	bool GetPutBackPromptState(FString& OutText, float& OutFacingDot, bool& bOutVisible) const;

	// Test-only: inject an item into the inventory by loading a static mesh by
	// asset path and feeding it through AddItemWithData. Lets focused inventory
	// tests skip the gameplay flow that normally grants the item.
	bool AddTestItem(FName ItemId, const FString& MeshAssetPath, FVector Scale = FVector(1.0f));

	// Test-only: add every UItemDefinition asset under FolderPath (e.g.
	// "/Game/Inventory") to the inventory. Returns count added.
	int32 AddAllItemDefsFromFolder(const FString& FolderPath);

	// Test-only: activate the blank tape without playing through the money
	// beat. Finds the level's USpawnerActorComponent and calls
	// ActivateChosenTape, exactly as Seneca does on receiving money.
	bool ActivateBlankTapeForTest();

	// Test-only: collect the blank tape through the production capture path
	// (CollectInspectedMovie) without aiming at it on the crowded shelf. The
	// shelf-aim flow is exercised by HappyPath; pose-focused tests use this.
	bool CollectBlankTapeForTest();

	// --- Car ride ---

	// Test-only: force the car ride to start regardless of the editor
	// play-location setting (E2E runs normally take the SkipRide path).
	bool ForceStartCarRide();

	// Test-only: drive the ride's EndRide path (fade + teleport + cleanup).
	bool EndCarRideNow();

	// Returns the runtime-spawned scenery conveyor actor (tag CarRideScenery),
	// or null if not spawned.
	AActor* FindCarRideConveyor() const;

	// --- Sensitivity / look diagnostics ---

	// Directly write the gamepad look sensitivity (clamps + snaps internally).
	void SetGamepadLookSensitivity(float Value);

	// Directly write the mouse look sensitivity (clamps + snaps internally).
	void SetMouseLookSensitivity(float Value);

	// Returns the current player ControlRotation yaw in degrees.
	float GetControllerYaw() const;

	// --- Test status overlay ---

	// Pins a status line on screen via GEngine->AddOnScreenDebugMessage with a
	// fixed key so each call replaces the previous line. Use to show the
	// current test step when running in editor.
	void SetTestStatus(const FString& Step);

private:
	UInventoryComponent* GetInventoryComponent() const;
	UInventoryUIComponent* GetInventoryUIComponent() const;
	UKeypadUIComponent* GetKeypadUIComponent() const;

	TSet<TWeakObjectPtr<AMovieBox>> CollectedMovies;
	TWeakObjectPtr<AMovieBox> LastFoundMovie;

	// Original transforms of actors moved by StageActorAtWaypoint, keyed by label.
	TMap<FString, FTransform> StagedActorTransforms;
};
