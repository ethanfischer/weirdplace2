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

	// --- State queries ---

	EPlayerActivityState GetActivityState() const;
	bool IsInSimpleDialogue() const;
	bool IsInAnyDialogue() const;
	bool HasItem(FName ItemId) const;
	int32 GetInventoryCount() const;

	// --- Sensitivity / look diagnostics ---

	// Directly write the gamepad look sensitivity (clamps + snaps internally).
	void SetGamepadLookSensitivity(float Value);

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
