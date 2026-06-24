#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UTexture2D;
class USoundBase;

// Visual data captured from collected items
USTRUCT(BlueprintType)
struct FInventoryItemData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName ItemID;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<UMaterialInterface*> Materials;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FVector Scale = FVector::OneVector;

	// Optional thumbnail override; if set, InventoryUIActor uses this instead of the convention path
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	UTexture2D* Thumbnail = nullptr;

	bool IsValid() const { return !ItemID.IsNone() && Mesh != nullptr; }
};

// Delegate for inventory change notifications - Blueprint-bindable
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryChanged, const TArray<FName>&, CurrentItems);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UInventoryComponent : public UActorComponent {
	GENERATED_BODY()

public:
	UInventoryComponent();

	// Sound played when an item is added to inventory
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Audio")
	USoundBase* CollectSound = nullptr;

	// Delegate that fires when inventory contents change
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

protected:
	virtual void BeginPlay() override;

public:
	// Adds an item with visual data (preferred - captures mesh/materials from source actor)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItemWithData(const FInventoryItemData& ItemData);

	// Adds an item by ID only (legacy - no visual data)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItem(const FName& ItemID);

	// Removes an item from the inventory
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(const FName& ItemID);

	// Checks if an item is in the inventory
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool HasItem(const FName& ItemID) const;

	// Returns copy of current inventory item IDs.
	// Sparse: entries may be NAME_None to preserve slot positions after RemoveItem.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	TArray<FName> GetItems() const;

	// Returns visual data for an item (nullptr if not found or no data)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	FInventoryItemData GetItemData(const FName& ItemID) const;

	// Returns count of items in inventory
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCount() const;

	// Overrides the thumbnail for an existing inventory item and broadcasts OnInventoryChanged
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UpdateItemThumbnail(const FName& ItemID, UTexture2D* NewThumbnail);

	// Helper to create item data from a static mesh component
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	static FInventoryItemData CreateItemDataFromMeshComponent(const FName& ItemID, UStaticMeshComponent* MeshComponent);

private:
	// Inventory items by slot index. Sparse: removed slots are NAME_None so
	// remaining items keep their grid position (Ocarina-of-Time style).
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TArray<FName> Items;

	// Visual data for each item
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TMap<FName, FInventoryItemData> ItemDataMap;
};
