#include "Inventory.h"
#include "GameFramework/Actor.h"

// Sets default values for this component
UInventoryComponent::UInventoryComponent() {
    PrimaryComponentTick.bCanEverTick = false; // No need to tick every frame
}

// Called when the game starts
void UInventoryComponent::BeginPlay() {
    Super::BeginPlay();
}

// Adds an item to the inventory
void UInventoryComponent::AddItem(EInventoryItem Item) {
    Inventory.Add(static_cast<int32>(Item));
    UE_LOG(LogTemp, Log, TEXT("Added item %d to inventory."), static_cast<int32>(Item));
}

// Removes an item from the inventory
bool UInventoryComponent::RemoveItem(EInventoryItem Item) {
    int32 ItemValue = static_cast<int32>(Item);
    if (Inventory.Contains(ItemValue)) {
        Inventory.RemoveSingle(ItemValue);
        UE_LOG(LogTemp, Log, TEXT("Removed item %d from inventory."), ItemValue);
        return true;
    }
    UE_LOG(LogTemp, Warning, TEXT("Item %d not found in inventory."), ItemValue);
    return false;
}

// Checks if an item is in the inventory
bool UInventoryComponent::HasItem(EInventoryItem Item) const {
    return Inventory.Contains(static_cast<int32>(Item));
}

// Displays inventory contents (for debugging)
void UInventoryComponent::DisplayInventory() const {
    FString InventoryString;
    for (int32 Item : Inventory) {
        InventoryString += FString::Printf(TEXT("%d "), Item);
    }
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Current Inventory: %s"), *InventoryString));
    UE_LOG(LogTemp, Log, TEXT("Current Inventory: %s"), *InventoryString);
}
