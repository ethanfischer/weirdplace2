#include "Inventory.h"
#include "GameFramework/Actor.h"

UInventoryComponent::UInventoryComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay() {
    Super::BeginPlay();
}

void UInventoryComponent::AddItem(const FName& ItemID) {
    if (ItemID.IsNone()) {
        UE_LOG(LogTemp, Warning, TEXT("AddItem: Cannot add item with None ID"));
        return;
    }

    Items.Add(ItemID);
    OnInventoryChanged.Broadcast(Items);
}

bool UInventoryComponent::RemoveItem(const FName& ItemID) {
    int32 Index = Items.Find(ItemID);
    if (Index != INDEX_NONE) {
        Items.RemoveAt(Index);

        // Clear active item if it was the removed item
        if (ActiveItem == ItemID) {
            ClearActiveItem();
        }

        OnInventoryChanged.Broadcast(Items);
        return true;
    }
    UE_LOG(LogTemp, Warning, TEXT("Item '%s' not found in inventory."), *ItemID.ToString());
    return false;
}

bool UInventoryComponent::HasItem(const FName& ItemID) const {
    return Items.Contains(ItemID);
}

TArray<FName> UInventoryComponent::GetItems() const {
    return Items;
}

int32 UInventoryComponent::GetItemCount() const {
    return Items.Num();
}

void UInventoryComponent::SetActiveItem(const FName& ItemID) {
    // Only set active if item is in inventory (or allow None to clear)
    if (ItemID.IsNone() || Items.Contains(ItemID)) {
        if (ActiveItem != ItemID) {
            ActiveItem = ItemID;
            OnActiveItemChanged.Broadcast(ActiveItem);
        }
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Cannot set active item '%s' - not in inventory"), *ItemID.ToString());
    }
}

FName UInventoryComponent::GetActiveItem() const {
    return ActiveItem;
}

void UInventoryComponent::ClearActiveItem() {
    if (!ActiveItem.IsNone()) {
        ActiveItem = NAME_None;
        OnActiveItemChanged.Broadcast(ActiveItem);
    }
}