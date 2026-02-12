#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HeldItemComponent.generated.h"

class UInventoryComponent;
class UInventoryUIComponent;
class UStaticMeshComponent;
class UCameraComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UHeldItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeldItemComponent();

	// Show the held item mesh
	UFUNCTION(BlueprintCallable, Category = "Held Item")
	void ShowHeldItem();

	// Hide the held item mesh
	UFUNCTION(BlueprintCallable, Category = "Held Item")
	void HideHeldItem();

	// Check if held item is currently visible
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Held Item")
	bool IsHeldItemVisible() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Configuration ---

	// Offset from character root (Forward, Right, Up) - diegetic positioning
	// Positioned as if held in right hand at chest/hip level
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held Item|Position")
	FVector HeldItemOffset = FVector(30.0f, 25.0f, 40.0f);

	// Rotation offset for held item (tilted to face up toward player)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held Item|Position")
	FRotator HeldItemRotation = FRotator(-45.0f, 0.0f, 0.0f);


private:
	// Mesh component for the held VHS box
	UPROPERTY()
	UStaticMeshComponent* HeldItemMesh;

	// Cached reference to owner's inventory component
	UPROPERTY()
	UInventoryComponent* InventoryComponent;

	// Cached reference to owner's inventory UI component
	UPROPERTY()
	UInventoryUIComponent* InventoryUIComponent;

	// Cached reference to camera for attachment
	UPROPERTY()
	UCameraComponent* CameraComponent;

	// Currently displayed item ID
	FName CurrentItemID;

	// Track previous inventory open state for edge detection
	bool bWasInventoryOpen = false;

	// Update the held item mesh/materials/scale from stored inventory data
	void UpdateHeldItem(const FName& ItemID);

	// Create the held item mesh component
	void CreateHeldItemMesh();

	// Callback when active item changes in inventory
	UFUNCTION()
	void OnActiveItemChanged(const FName& NewActiveItem);
};
