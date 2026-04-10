#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyCharacter.h"
#include "TestDriverSubsystem.generated.h"

class AFirstPersonCharacter;
class AMovieBox;
class APropActor;
class ASeneca;
class ATestWaypoint;
class UInventoryComponent;
class UInventoryUIComponent;

// Verb layer for E2E tests. Call these from latent automation commands to
// drive the player character through scripted scenarios. Each verb maps
// directly to the real gameplay function the player input would invoke —
// we skip the Enhanced Input layer but not the gameplay logic underneath.
UCLASS()
class WEIRDPLACE2_API UTestDriverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Character access ---

	AFirstPersonCharacter* GetPlayer() const;
	bool IsPlayerReady() const;

	// --- Movement / look ---

	// Teleports the player character to the waypoint's location and points
	// the camera at the waypoint's forward vector. Returns false if not found.
	bool TeleportPlayerToWaypoint(FName WaypointTag);

	// Rotates the player controller to look at a target actor.
	bool LookAt(AActor* Target);

	// Finds an actor by editor label (falls back to internal name) and aims
	// the camera at it. Returns false if not found.
	bool LookAtActorByLabel(const FString& Label);

	// Returns the first actor whose editor label (or internal name) matches,
	// or nullptr. No side effects.
	AActor* FindActorByLabel(const FString& Label) const;

	// Returns the single ASeneca instance in the level, or nullptr.
	ASeneca* FindSeneca() const;

	// --- Interaction ---

	// Mirrors AFirstPersonCharacter::HandleInteractTriggered: advances dialogue
	// if in dialogue, otherwise runs the interaction trace and fires Interact
	// on the hit actor. Going through HandleInteractTriggered directly would
	// be ideal, but that method is private; we replicate its branching here
	// while still calling the real gameplay APIs it calls.
	void PressInteract();

	// Directly fires Interact on the given actor, bypassing the camera trace.
	// Used by E2E tests so they don't need the player positioned/aimed
	// correctly — the gameplay Interact_Implementation still runs. Returns
	// false if the actor is null or doesn't implement IInteractable.
	bool InteractWith(AActor* Actor);

	// Convenience: find Seneca and fire Interact on him.
	bool InteractWithSeneca();

	// Convenience: find an actor by label and fire Interact on it.
	bool InteractWithActorByLabel(const FString& Label);

	// Returns the actor the interaction trace currently hits, or nullptr.
	AActor* GetFocusedInteractable() const;

	// --- Inventory ---

	void OpenInventory();
	void CloseInventory();

	// True when the inventory open animation has finished.
	bool IsInventoryFullyOpen() const;

	// True when the inventory close animation has finished.
	// Tests should wait for this after CloseInventory so CanInteract is restored
	// before they try to PressInteract again.
	bool IsInventoryFullyClosed() const;

	// Sets the inventory selection to the given slot index and commits it
	// (same as pressing the confirm button in the UI). Requires the inventory
	// to be fully open.
	bool SelectInventoryItemByIndex(int32 Index);

	// --- Movie collection ---

	// Finds any uncollected AMovieBox anywhere in the level, enters inspection,
	// and immediately collects it. Already-collected boxes are tracked
	// internally so consecutive calls pick different movies. We don't filter
	// by distance because this path bypasses the interaction trace — the goal
	// is smoke-testing movie pickup flow, not level geometry. Returns the
	// collected box, or nullptr on failure.
	AMovieBox* CollectNextMovie();

	// --- State queries ---

	EPlayerActivityState GetActivityState() const;
	bool IsInSimpleDialogue() const;
	bool IsInAnyDialogue() const;
	bool HasItem(FName ItemId) const;
	int32 GetInventoryCount() const;

private:
	UInventoryComponent* GetInventoryComponent() const;
	UInventoryUIComponent* GetInventoryUIComponent() const;

	// Movies the test has already collected this session. Used to skip them
	// when CollectNextMovie is called repeatedly from the same vantage point.
	// Weak pointers don't need UPROPERTY tracking.
	TSet<TWeakObjectPtr<AMovieBox>> CollectedMovies;
};
