#include "Inventory.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

UInventoryComponent::UInventoryComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay() {
    Super::BeginPlay();
}

void UInventoryComponent::AddItemWithData(const FInventoryItemData& ItemData) {
    if (ItemData.ItemID.IsNone()) {
        UE_LOG(LogTemp, Warning, TEXT("AddItemWithData: Cannot add item with None ID"));
        return;
    }

    Items.Add(ItemData.ItemID);
    ItemDataMap.Add(ItemData.ItemID, ItemData);
    if (CollectSound)
    {
        UGameplayStatics::PlaySound2D(this, CollectSound);
    }
    OnInventoryChanged.Broadcast(Items);
}

void UInventoryComponent::AddItem(const FName& ItemID) {
    if (ItemID.IsNone()) {
        UE_LOG(LogTemp, Warning, TEXT("AddItem: Cannot add item with None ID"));
        return;
    }

    Items.Add(ItemID);
    // No visual data for legacy AddItem - create empty entry
    FInventoryItemData EmptyData;
    EmptyData.ItemID = ItemID;
    ItemDataMap.Add(ItemID, EmptyData);
    if (CollectSound)
    {
        UGameplayStatics::PlaySound2D(this, CollectSound);
    }
    OnInventoryChanged.Broadcast(Items);
}

bool UInventoryComponent::RemoveItem(const FName& ItemID) {
    int32 Index = Items.Find(ItemID);
    if (Index != INDEX_NONE) {
        Items.RemoveAt(Index);
        ItemDataMap.Remove(ItemID);

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

FInventoryItemData UInventoryComponent::GetItemData(const FName& ItemID) const {
    if (const FInventoryItemData* Data = ItemDataMap.Find(ItemID)) {
        return *Data;
    }
    return FInventoryItemData();
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

FInventoryItemData UInventoryComponent::GetActiveItemData() const {
    return GetItemData(ActiveItem);
}

void UInventoryComponent::ClearActiveItem() {
    if (!ActiveItem.IsNone()) {
        ActiveItem = NAME_None;
        OnActiveItemChanged.Broadcast(ActiveItem);
    }
}

void UInventoryComponent::UpdateItemThumbnail(const FName& ItemID, UTexture2D* NewThumbnail)
{
    FInventoryItemData* Data = ItemDataMap.Find(ItemID);
    if (!Data)
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateItemThumbnail: Item '%s' not found in inventory"), *ItemID.ToString());
        return;
    }
    Data->Thumbnail = NewThumbnail;
    OnInventoryChanged.Broadcast(Items);
}

FInventoryItemData UInventoryComponent::CreateItemDataFromMeshComponent(const FName& ItemID, UStaticMeshComponent* MeshComponent) {
    FInventoryItemData Data;
    Data.ItemID = ItemID;

    if (MeshComponent) {
        Data.Mesh = MeshComponent->GetStaticMesh();
        Data.Scale = MeshComponent->GetRelativeScale3D();
        Data.Rotation = MeshComponent->GetRelativeRotation();

        // Capture all materials
        int32 NumMaterials = MeshComponent->GetNumMaterials();
        for (int32 i = 0; i < NumMaterials; i++) {
            Data.Materials.Add(MeshComponent->GetMaterial(i));
        }
    }

    return Data;
}