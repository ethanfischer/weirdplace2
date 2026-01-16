#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory.h"
#include "InventoryItemMapping.h"
#include "InventoryRoomComponent.generated.h"

class UInventoryComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UInventoryRoomComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryRoomComponent();

	// Toggle inventory room (called from input binding)
	UFUNCTION(BlueprintCallable, Category = "Inventory Room")
	void ToggleInventoryRoom();

	// Check if currently in inventory room
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Room")
	bool IsInInventoryRoom() const { return bIsInInventoryRoom; }

protected:
	virtual void BeginPlay() override;

	// --- Configuration Properties ---

	// Target point actor to teleport to (set in editor). If not set, uses InventoryRoomLocation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Location")
	AActor* InventoryRoomTarget;

	// Fallback location if no target point is set
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Location")
	FVector InventoryRoomLocation = FVector(50000.0f, 0.0f, 0.0f);

	// Rotation when entering inventory room (used if no target point, or as override)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Location")
	FRotator InventoryRoomRotation = FRotator::ZeroRotator;

	// Folder path for spawned inventory items in Outliner
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	FName InventoryFolderPath = FName("Inventory");

	// Mapping of inventory items to display actors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Items")
	TArray<FInventoryItemDisplayInfo> ItemDisplayMappings;

	// Grid layout settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	int32 GridColumns = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	float GridSpacing = 100.0f;

	// Height offset for displayed items (relative to room location)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	float ItemDisplayHeight = 50.0f;

	// Distance in front of player spawn point to display items
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	float ItemDisplayDistance = 200.0f;

private:
	// State tracking
	bool bIsInInventoryRoom = false;

	// Stored transform before entering inventory room
	FVector StoredLocation;
	FRotator StoredRotation;

	// Reference to owner's inventory component
	UPROPERTY()
	UInventoryComponent* InventoryComponent;

	// Currently spawned display actors
	UPROPERTY()
	TArray<AActor*> SpawnedDisplayActors;

	// --- Internal Methods ---
	void TeleportToInventoryRoom();
	void TeleportBack();
	void SpawnInventoryDisplayActors();
	void DestroyInventoryDisplayActors();

	// Find display info for given inventory item
	const FInventoryItemDisplayInfo* GetDisplayInfo(EInventoryItem Item) const;

	// Calculate spawn position for item at given index
	FVector CalculateItemPosition(int32 Index) const;

	// Handle inventory changes while in room (refresh display)
	UFUNCTION()
	void OnInventoryChanged(const TArray<EInventoryItem>& CurrentInventory);
};
