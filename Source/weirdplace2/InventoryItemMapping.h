#pragma once

#include "CoreMinimal.h"
#include "InventoryItemMapping.generated.h"

/**
 * Struct to map item IDs (FName) to actor classes for 3D display
 * Configurable in Blueprint/Editor
 */
USTRUCT(BlueprintType)
struct FInventoryItemDisplayInfo
{
	GENERATED_BODY()

	// The inventory item ID (e.g., "ALIEN", "KEY_BASEMENT")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Display")
	FName ItemID;

	// The actor class to spawn for this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Display")
	TSubclassOf<AActor> DisplayActorClass;

	// Optional scale override for the spawned actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Display")
	FVector DisplayScale = FVector(1.0f, 1.0f, 1.0f);

	// Optional rotation offset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Display")
	FRotator DisplayRotation = FRotator::ZeroRotator;

	FInventoryItemDisplayInfo()
		: ItemID(NAME_None)
		, DisplayActorClass(nullptr)
	{}
};
