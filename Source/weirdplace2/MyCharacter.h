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

UCLASS()
class WEIRDPLACE2_API AMyCharacter : public ACharacter {
	GENERATED_BODY()

public:
	AMyCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void SetCanInteract(bool value);
	bool GetCanInteract() const { return CanInteract; }

	// Unlocks inventory access (called by Seneca after first dialogue)
	void UnlockInventory();
	bool IsInventoryUnlocked() const { return bInventoryUnlocked; }

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

	bool bInventoryUnlocked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryComponent* InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryUIComponent* InventoryUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UHeldItemComponent* HeldItemComponent;

	// Input callback for Tab key - toggle inventory UI
	void OnToggleInventory();
};
