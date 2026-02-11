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

	// Offset from camera (Forward, Right, Down relative to camera)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held Item|Position")
	FVector HeldItemOffset = FVector(50.0f, 20.0f, -15.0f);

	// Rotation offset for held item
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held Item|Position")
	FRotator HeldItemRotation = FRotator(-10.0f, -20.0f, 5.0f);

	// Scale for held item (VHS box proportions - Width, Height, Depth)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held Item|Position")
	FVector HeldItemScale = FVector(1.0f, 7.0f, 10.0f);

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

	// Update the held item mesh material based on item ID
	void UpdateHeldItemMaterial(const FName& ItemID);

	// Create the held item mesh component
	void CreateHeldItemMesh();

	// Callback when active item changes in inventory
	UFUNCTION()
	void OnActiveItemChanged(const FName& NewActiveItem);
};
