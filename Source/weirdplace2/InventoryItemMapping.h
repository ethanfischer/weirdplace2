#pragma once

#include "CoreMinimal.h"
#include "Inventory.h"
#include "InventoryItemMapping.generated.h"

/**
 * Struct to map EInventoryItem enum values to actor classes for 3D display
 * Configurable in Blueprint/Editor
 */
USTRUCT(BlueprintType)
struct FInventoryItemDisplayInfo
{
	GENERATED_BODY()

	// The inventory item type
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Display")
	EInventoryItem ItemType;

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
		: ItemType(EInventoryItem::InventoryItem1)
		, DisplayActorClass(nullptr)
	{}
};
