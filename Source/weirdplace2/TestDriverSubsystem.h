#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
enum class EPlayerActivityState : uint8;
#include "TestDriverSubsystem.generated.h"

class AFirstPersonCharacter;
class AMovieBox;
class APropActor;
class AHudson;
class ARick;
class ASeneca;
class ATestWaypoint;
class UInputAction;
class UInventoryComponent;
class UInventoryUIComponent;

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

	// Set by FTD_TeleportNearBlankTape: a verified-hittable point just inside
	// the blank tape's collision surface. FTD_LookAtBlankTape aims here —
	// derived centers (actor bounds, envelope bounds) miss the Memphis mesh's
	// collision at some shelf-slot angles.
	FVector BlankTapeAimPoint = FVector::ZeroVector;
	bool LookAtSeneca();
	bool LookAtRick();
	bool LookAtKeyActor();

	AActor* FindActorByLabel(const FString& Label) const;
	ASeneca* FindSeneca() const;
	ARick* FindRick() const;
	AHudson* FindHudson() const;
	bool LookAtHudson();

	// --- Seneca test helpers ---

	// Skip the 60-second SmokingAppearDelay so the E2E test doesn't have to wait.
	void FastForwardSenecaSmoking();

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

	// --- State queries ---

	EPlayerActivityState GetActivityState() const;
	bool IsInSimpleDialogue() const;
	bool IsInAnyDialogue() const;
	bool HasItem(FName ItemId) const;
	int32 GetInventoryCount() const;

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

	// Camera-space directions of the held item's longest and shortest local
	// bounding-box axes, the scaled length of the longest half-extent in cm,
	// and the bounds center in camera space. Two box-shaped items "held in
	// the same pose" have matching axes, comparable size, and the same
	// center, regardless of how their meshes were authored. Returns false if
	// nothing is held/visible.
	bool GetHeldItemBoxAxes(FVector& OutLongAxisCamSpace, FVector& OutShortAxisCamSpace, float& OutMaxExtent, FVector& OutCenterCamSpace) const;

	// Test-only: inject an item into the inventory by loading a static mesh by
	// asset path and feeding it through AddItemWithData. Lets focused inventory
	// tests skip the gameplay flow that normally grants the item.
	bool AddTestItem(FName ItemId, const FString& MeshAssetPath, FVector Scale = FVector(1.0f));

	// Test-only: add every UItemDefinition asset under FolderPath (e.g.
	// "/Game/Inventory") to the inventory. Returns count added.
	int32 AddAllItemDefsFromFolder(const FString& FolderPath);

	// Test-only: override the held-item slot pose (relative to camera) so the
	// item is visible during a screenshot tour, regardless of the production
	// hand-rig position.
	bool SetHeldItemSlotPose(FVector Offset, FRotator Rotation);

	// Test-only: activate the blank tape without playing through the money
	// beat. Finds the level's USpawnerActorComponent and calls
	// ActivateChosenTape, exactly as Seneca does on receiving money.
	bool ActivateBlankTapeForTest();

	// Test-only: collect the blank tape through the production capture path
	// (CollectInspectedMovie) without aiming at it on the crowded shelf. The
	// shelf-aim flow is exercised by HappyPath; pose-focused tests use this.
	bool CollectBlankTapeForTest();

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

	TSet<TWeakObjectPtr<AMovieBox>> CollectedMovies;
	TWeakObjectPtr<AMovieBox> LastFoundMovie;
};
