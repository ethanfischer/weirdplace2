// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MyCharacter.generated.h"

class UInventoryComponent;
class UInventoryUIComponent;
class UHeldItemComponent;
class UStaticMeshComponent;
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
	virtual void Tick(float DeltaTime) override;

	void SetCanInteract(bool value);
	bool GetCanInteract() const { return CanInteract; }

	void SetActivityState(EPlayerActivityState NewState);
	EPlayerActivityState GetActivityState() const { return ActivityState; }
	bool IsInAnyDialogue() const;
	bool IsDialogueCooldownActive() const;

	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;

	// Unlocks inventory access (called by Seneca after first dialogue)
	void UnlockInventory();
	bool IsInventoryUnlocked() const { return bInventoryUnlocked; }

	// Locks movie collection (called by Seneca when checkout begins)
	void LockMovieCollection();
	bool IsMovieCollectionLocked() const { return bMovieCollectionLocked; }

	// Add item to inventory by ID (legacy - no visual data)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory")
	void AddItemToInventory(const FName& ItemID);
	virtual void AddItemToInventory_Implementation(const FName& ItemID);

	// Add item to inventory with visual data captured from mesh component
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItemToInventoryWithMesh(const FName& ItemID, UStaticMeshComponent* MeshComponent);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryUIComponent* GetInventoryUIComponent() const { return InventoryUIComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UHeldItemComponent* GetHeldItemComponent() const { return HeldItemComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	bool CanInteract = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	EPlayerActivityState ActivityState = EPlayerActivityState::FreeRoaming;

	double LastDialogueEndTime = -TNumericLimits<double>::Max();

	bool bInventoryUnlocked = false;
	bool bMovieCollectionLocked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryComponent* InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryUIComponent* InventoryUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UHeldItemComponent* HeldItemComponent;
};
