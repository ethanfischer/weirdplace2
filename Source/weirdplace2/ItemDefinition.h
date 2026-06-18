#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Inventory.h"
#include "ItemDefinition.generated.h"

class UStaticMesh;
class UMaterialInterface;

UCLASS(BlueprintType)
class WEIRDPLACE2_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Visual")
	UStaticMesh* Mesh = nullptr;

	// Empty falls back to the mesh's authored materials.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Visual")
	TArray<UMaterialInterface*> MaterialOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Visual")
	FVector Scale = FVector::OneVector;

	// Composed with HeldItemComponent::HeldItemRotation when this item is held.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Pose")
	FRotator HeldRotation = FRotator::ZeroRotator;

	// Rotation for the pickup-notification mesh popup.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Pose")
	FRotator NotificationRotation = FRotator::ZeroRotator;

	// Local-space rotation applied on top of camera-facing when an
	// AInspectablePickup pulls this item in front of the camera.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Pose")
	FRotator InspectionRotation = FRotator::ZeroRotator;

	UFUNCTION(BlueprintCallable, Category = "Item")
	FInventoryItemData ToInventoryItemData() const;
};
