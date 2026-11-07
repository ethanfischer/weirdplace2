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

	// Rotation for the pickup-notification mesh popup.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Pose")
	FRotator NotificationRotation = FRotator::ZeroRotator;

	// Local-space rotation applied on top of camera-facing when an
	// AInspectablePickup pulls this item in front of the camera.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Pose")
	FRotator InspectionRotation = FRotator::ZeroRotator;

	// Played at the target when this item is used on something that doesn't
	// react (IInteractable::UseItem returned false). None = silent.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Use")
	USoundBase* UseFailSound = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Item")
	FInventoryItemData ToInventoryItemData() const;

	// Every quest/tool item (Key, Money, MeatTenderizer, ...) has a definition at
	// /Game/Inventory/DA_<ItemID>. Movies are captured off a movie box and have
	// none — that absence is what makes an item a movie (see ASeneca::IsMovieItem).
	static bool Exists(FName ItemID);

	// The definition for ItemID, or null if it has none (a movie).
	static UItemDefinition* Find(FName ItemID);
};
