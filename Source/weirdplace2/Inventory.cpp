#include "Inventory.h"
#include "GameFramework/Actor.h"

UInventoryComponent::UInventoryComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay() {
    Super::BeginPlay();
}

void UInventoryComponent::AddItem(EInventoryItem Item) {
    Inventory.Add(Item);
    UE_LOG(LogTemp, Log, TEXT("Added item %d to inventory on %s (ptr: %p). Total items: %d"),
        static_cast<int32>(Item), *GetOwner()->GetName(), this, Inventory.Num());
    OnInventoryChanged.Broadcast(Inventory);
}

bool UInventoryComponent::RemoveItem(EInventoryItem Item) {
    int32 Index = Inventory.Find(Item);
    if (Index != INDEX_NONE) {
        Inventory.RemoveAt(Index);
        UE_LOG(LogTemp, Log, TEXT("Removed item %d from inventory."), static_cast<int32>(Item));
        OnInventoryChanged.Broadcast(Inventory);
        return true;
    }
    UE_LOG(LogTemp, Warning, TEXT("Item %d not found in inventory."), static_cast<int32>(Item));
    return false;
}

bool UInventoryComponent::HasItem(EInventoryItem Item) const {
    return Inventory.Contains(Item);
}

void UInventoryComponent::DisplayInventory() const {
    FString InventoryString;
    for (EInventoryItem Item : Inventory) {
        InventoryString += FString::Printf(TEXT("%d "), static_cast<int32>(Item));
    }
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Current Inventory: %s"), *InventoryString));
    UE_LOG(LogTemp, Log, TEXT("Current Inventory: %s"), *InventoryString);
}

TArray<EInventoryItem> UInventoryComponent::GetInventoryItems() const {
    return Inventory;
}

int32 UInventoryComponent::GetInventoryCount() const {
    return Inventory.Num();
}
