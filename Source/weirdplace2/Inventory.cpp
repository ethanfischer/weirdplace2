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

// Place ItemID into the first NAME_None hole, or append. Returns the slot index used.
static int32 AssignToFirstFreeSlot(TArray<FName>& Items, const FName& ItemID) {
    for (int32 i = 0; i < Items.Num(); ++i) {
        if (Items[i].IsNone()) {
            Items[i] = ItemID;
            return i;
        }
    }
    return Items.Add(ItemID);
}

void UInventoryComponent::AddItemWithData(const FInventoryItemData& ItemData) {
    if (ItemData.ItemID.IsNone()) {
        UE_LOG(LogTemp, Warning, TEXT("AddItemWithData: Cannot add item with None ID"));
        return;
    }

    AssignToFirstFreeSlot(Items, ItemData.ItemID);
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

    AssignToFirstFreeSlot(Items, ItemID);
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
        // Leave the slot in place (NAME_None) so other items don't shift their grid position.
        Items[Index] = NAME_None;
        ItemDataMap.Remove(ItemID);

        // Trim trailing empty slots so Items.Num() doesn't grow unboundedly.
        while (Items.Num() > 0 && Items.Last().IsNone()) {
            Items.Pop(EAllowShrinking::No);
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
    int32 Count = 0;
    for (const FName& ItemID : Items) {
        if (!ItemID.IsNone()) {
            ++Count;
        }
    }
    return Count;
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

        // Capture all materials
        int32 NumMaterials = MeshComponent->GetNumMaterials();
        for (int32 i = 0; i < NumMaterials; i++) {
            Data.Materials.Add(MeshComponent->GetMaterial(i));
        }
    }

    return Data;
}