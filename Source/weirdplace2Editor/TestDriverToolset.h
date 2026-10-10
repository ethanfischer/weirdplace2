#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "TestDriverToolset.generated.h"

class UTestDriverSubsystem;
class UToolCallAsyncResultVoid;
class UToolCallAsyncResultString;

/**
 * Snapshot of the player's interaction state in the running PIE session,
 * returned by GetPlayerStatus.
 */
USTRUCT(BlueprintType)
struct FTestDriverPlayerStatus
{
	GENERATED_BODY()

	/** Player activity state (FreeRoaming, Interacting, InSimpleDialogue, InDialogue). */
	UPROPERTY() FString ActivityState;

	/** Current pause-menu page (Pause, Settings, Graphics, Tunables). Empty when the menu is closed. */
	UPROPERTY() FString MenuPage;

	/** True while the inventory UI is fully open. */
	UPROPERTY() bool bInventoryOpen = false;

	/** Player pawn world location in cm. */
	UPROPERTY() FVector Location = FVector::ZeroVector;
};

/**
 * Drives the game live in a Play-In-Editor session through the same input
 * pipeline the E2E tests use: injecting the player's Enhanced Input actions,
 * simulating keys, teleporting, aiming the camera, and querying interaction
 * state. Requires an active PIE session; every tool raises without one.
 */
UCLASS(BlueprintType, MinimalAPI)
class UTestDriverToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Presses one of the player's Enhanced Input actions and releases it on the
	 * next frame, firing its Started trigger exactly once.
	 * @param ActionName One of: Interact, Inventory, Settings, NextOption,
	 *                   PreviousOption, NavigateLeft, NavigateRight, Back.
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static void PressInputAction(const FString& ActionName);

	/**
	 * Presses a sequence of Enhanced Input actions with a fixed delay between
	 * presses, completing after the last release — one call per scenario step
	 * instead of one call per keypress.
	 * @param ActionNames Same names as PressInputAction, in press order.
	 * @param DelayBetween Seconds between consecutive presses. 0.25 suits menu navigation.
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static UToolCallAsyncResultVoid* PressInputSequence(const TArray<FString>& ActionNames, float DelayBetween = 0.25f);

	/**
	 * Completes when the player reaches the given activity state; errors on timeout.
	 * @param State FreeRoaming, Interacting, InSimpleDialogue, or InDialogue.
	 * @param TimeoutSeconds Seconds to wait before failing.
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static UToolCallAsyncResultVoid* WaitForActivityState(const FString& State, float TimeoutSeconds = 10.f);

	/**
	 * Completes when the pause menu is fully open on the given page; errors on timeout.
	 * @param Page Pause, Settings, Graphics, or Tunables.
	 * @param TimeoutSeconds Seconds to wait before failing.
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static UToolCallAsyncResultVoid* WaitForMenuPage(const FString& Page, float TimeoutSeconds = 10.f);

	/**
	 * Sets the pressed state of one of the player's Enhanced Input actions
	 * without an automatic release, for holds spanning multiple frames.
	 * @param ActionName Same names as PressInputAction.
	 * @param bPressed True to press, false to release.
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static void SetInputActionPressed(const FString& ActionName, bool bPressed);

	/**
	 * Presses a keyboard key through APlayerController::InputKey (legacy
	 * BindAction path) and releases it on the next frame.
	 * @param KeyName An FKey name, e.g. "E", "Q", "Escape".
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static void PressKey(const FString& KeyName);

	/**
	 * Returns the player's current interaction state.
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static FTestDriverPlayerStatus GetPlayerStatus();

	/**
	 * Captures what the player currently sees in the PIE viewport (diegetic UI
	 * included, editor chrome excluded) as a PNG on disk, and returns the
	 * absolute file path. Read the returned path to view the image. Unlike the
	 * editor-camera CaptureViewport, this is the first-person game view. A file
	 * path (not inline base64) because the image-content transport is capped
	 * well below a readable frame.
	 * @param MaxDimension Longest edge of the written image in pixels; the
	 *   capture is scaled down to fit, preserving aspect ratio.
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static UToolCallAsyncResultString* CapturePlayerView(int32 MaxDimension = 1280);

	/**
	 * Teleports the player onto the ATestWaypoint with the given tag.
	 * @param WaypointTag The waypoint's tag, as used by the E2E tests.
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static void TeleportToWaypoint(const FString& WaypointTag);

	/**
	 * Teleports the player near an actor, facing it.
	 * @param ActorLabel The actor's editor label.
	 * @param Distance Standoff distance in cm.
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static void TeleportNearActor(const FString& ActorLabel, float Distance = 200.f);

	/**
	 * Aims the player camera at an actor.
	 * @param ActorLabel The actor's editor label.
	 */
	UFUNCTION(meta = (AICallable), Category = "TestDriver")
	static void LookAtActor(const FString& ActorLabel);

private:
	// The PIE world's driver subsystem. Raises and returns null without PIE.
	static UTestDriverSubsystem* GetDriverChecked();
	static class UInputAction* ResolveActionChecked(const FString& ActionName);
};
