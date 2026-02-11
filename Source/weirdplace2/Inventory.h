#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory.generated.h"

// Delegate for inventory change notifications - Blueprint-bindable
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryChanged, const TArray<FName>&, CurrentItems);

// Delegate for active item change notifications
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveItemChanged, const FName&, NewActiveItem);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UInventoryComponent : public UActorComponent {
	GENERATED_BODY()

public:
	UInventoryComponent();

	// Delegate that fires when inventory contents change
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	// Delegate that fires when active item changes
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnActiveItemChanged OnActiveItemChanged;

protected:
	virtual void BeginPlay() override;

public:
	// Adds an item to the inventory by ID (e.g., "ALIEN", "KEY_BASEMENT", "FLASHLIGHT")
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItem(const FName& ItemID);

	// Removes an item from the inventory
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(const FName& ItemID);

	// Checks if an item is in the inventory
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool HasItem(const FName& ItemID) const;

	// Returns copy of current inventory items
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	TArray<FName> GetItems() const;

	// Returns count of items in inventory
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCount() const;

	// Sets the currently active (selected) item
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetActiveItem(const FName& ItemID);

	// Gets the currently active (selected) item
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	FName GetActiveItem() const;

	// Clears the active item selection
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearActiveItem();

private:
	// All inventory items by ID
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TArray<FName> Items;

	// Currently selected/active item
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	FName ActiveItem;
};
