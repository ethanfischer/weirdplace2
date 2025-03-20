#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory.generated.h"

UENUM(BlueprintType)
enum class EInventoryItem : uint8 {
    InventoryItem1 UMETA(DisplayName = "Item 1"),
    InventoryItem2 UMETA(DisplayName = "Item 2"),
    InventoryItem3 UMETA(DisplayName = "Item 3"),
    InventoryItem4 UMETA(DisplayName = "Item 4"),
    InventoryItem5 UMETA(DisplayName = "Item 5"),
    InventoryItem6 UMETA(DisplayName = "Item 6"),
    InventoryItem7 UMETA(DisplayName = "Item 7"),
    InventoryItem8 UMETA(DisplayName = "Item 8"),
    InventoryItem9 UMETA(DisplayName = "Item 9"),
    InventoryItem10 UMETA(DisplayName = "Item 10"),
    InventoryItem11 UMETA(DisplayName = "Item 11")
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UInventoryComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UInventoryComponent();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

public:
    // Adds an item to the inventory
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void AddItem(EInventoryItem Item);

    // Removes an item from the inventory
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItem(EInventoryItem Item);

    // Checks if an item is in the inventory
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool HasItem(EInventoryItem Item) const;

    // Displays inventory (for debugging)
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void DisplayInventory() const;

private:
    TArray<int32> Inventory; // Using TArray instead of List<int>
};
