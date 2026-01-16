// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Inventory.h"
#include "MyCharacter.generated.h"

class UInventoryComponent;
class UInventoryRoomComponent;

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

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory")
	void         AddItemToInventory(EInventoryItem Item);
	virtual void AddItemToInventory_Implementation(EInventoryItem Item);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryRoomComponent* GetInventoryRoomComponent() const { return InventoryRoomComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	bool CanInteract = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryComponent* InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryRoomComponent* InventoryRoomComponent;

	// Legacy input callback for Tab key
	void OnToggleInventoryRoom();
};
