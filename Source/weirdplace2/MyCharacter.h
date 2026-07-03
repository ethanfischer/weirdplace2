// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MyCharacter.generated.h"

class UInventoryComponent;
class UInventoryUIComponent;
class UKeypadUIComponent;
struct FInventoryItemData;

UENUM(BlueprintType)
enum class EPlayerActivityState : uint8
{
	FreeRoaming            UMETA(DisplayName = "Free Roaming"),
	Interacting            UMETA(DisplayName = "Interacting"),
	InSimpleDialogue       UMETA(DisplayName = "In Simple Dialogue"),
	InDialogue                              UMETA(DisplayName = "In Dialogue")
};

UCLASS()
class WEIRDPLACE2_API AMyCharacter : public ACharacter {
	GENERATED_BODY()

public:
	AMyCharacter();

protected:
	virtual void BeginPlay() override;

public:
	void SetCanInteract(bool value);
	bool GetCanInteract() const { return CanInteract; }

	void SetActivityState(EPlayerActivityState NewState);
	EPlayerActivityState GetActivityState() const { return ActivityState; }
	bool IsInAnyDialogue() const;
	bool IsDialogueCooldownActive() const;

	// Enter an interaction "hold": freeze movement (and look if bFreezeLook),
	// disable environment interaction, set Interacting, and ensure the player
	// controller has an InputComponent so the caller can bind its own exit/rotate
	// actions. Shared by PayPhone / MovieBox / InspectablePickup; each caller still
	// binds its own actions afterward. No-op without a player controller.
	void BeginInteractionHold(bool bFreezeLook);

	// Reverse of BeginInteractionHold: unfreeze movement (and look if
	// bUnfreezeLook), re-enable interaction, return to FreeRoaming.
	void EndInteractionHold(bool bUnfreezeLook);

	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;

	// Locks movie collection (called by Seneca when checkout begins)
	void LockMovieCollection();
	bool IsMovieCollectionLocked() const { return bMovieCollectionLocked; }

	// One-time "put back" control hint: shown only until the player's first
	// completed movie interaction — collecting one or putting one back.
	bool HasInteractedWithMovie() const { return bHasInteractedWithMovie; }
	void MarkMovieInteraction() { bHasInteractedWithMovie = true; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryUIComponent* GetInventoryUIComponent() const { return InventoryUIComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Keypad")
	UKeypadUIComponent* GetKeypadUIComponent() const { return KeypadUIComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	bool CanInteract = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	EPlayerActivityState ActivityState = EPlayerActivityState::FreeRoaming;

	double LastDialogueEndTime = -TNumericLimits<double>::Max();

	bool bMovieCollectionLocked = false;

	// Set true on the player's first completed movie interaction (collect or
	// put-back); suppresses the put-back prompt on every subsequent inspection.
	// Runtime-only (resets each session), like bMovieCollectionLocked.
	bool bHasInteractedWithMovie = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryComponent* InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryUIComponent* InventoryUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Keypad", meta = (AllowPrivateAccess = "true"))
	UKeypadUIComponent* KeypadUIComponent;
};
