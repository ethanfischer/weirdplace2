#include "HeldItemComponent.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "MyCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "IXRTrackingSystem.h"

UHeldItemComponent::UHeldItemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UHeldItemComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Get cached references from owner
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(Owner))
	{
		InventoryComponent = MyCharacter->GetInventoryComponent();
		InventoryUIComponent = MyCharacter->GetInventoryUIComponent();
	}
	else
	{
		InventoryComponent = Owner->FindComponentByClass<UInventoryComponent>();
		InventoryUIComponent = Owner->FindComponentByClass<UInventoryUIComponent>();
	}

	// Find camera component
	CameraComponent = Owner->FindComponentByClass<UCameraComponent>();

	// Bind to active item changed delegate
	if (InventoryComponent)
	{
		InventoryComponent->OnActiveItemChanged.AddDynamic(this, &UHeldItemComponent::OnActiveItemChanged);

		// Check if there's already an active item
		FName ActiveItem = InventoryComponent->GetActiveItem();
		if (!ActiveItem.IsNone())
		{
			CurrentItemID = ActiveItem;
		}
	}

	// Create the mesh component
	CreateHeldItemMesh();

	// Initialize visibility state based on whether inventory is open
	if (InventoryUIComponent)
	{
		bWasInventoryOpen = InventoryUIComponent->IsInventoryOpen();
	}
}

void UHeldItemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!InventoryUIComponent) return;

	bool bIsInventoryOpen = InventoryUIComponent->IsInventoryOpen();

	// Detect edge transitions
	if (bWasInventoryOpen && !bIsInventoryOpen)
	{
		// Inventory just closed - show held item if we have one
		if (!CurrentItemID.IsNone())
		{
			UpdateHeldItem(CurrentItemID);
			ShowHeldItem();
		}
	}
	else if (!bWasInventoryOpen && bIsInventoryOpen)
	{
		// Inventory just opened - hide held item
		HideHeldItem();
	}

	bWasInventoryOpen = bIsInventoryOpen;
}

void UHeldItemComponent::CreateHeldItemMesh()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("HeldItemComponent: No owner!"));
		return;
	}

	// Create the mesh component (mesh/materials will be set when active item changes)
	HeldItemMesh = NewObject<UStaticMeshComponent>(Owner);
	if (!HeldItemMesh) return;

	HeldItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldItemMesh->SetCastShadow(false);

	// Check if VR mode is active
	bool bIsVRMode = GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();

	// Attach based on mode
	if (bIsVRMode)
	{
		// VR mode: TODO - attach to player's hand motion controller instead
		// For now, attach to root as placeholder
		HeldItemMesh->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		UE_LOG(LogTemp, Warning, TEXT("HeldItemComponent: VR mode - attached to root (TODO: attach to hand)"));
	}
	else
	{
		// Flatscreen mode: attach to camera
		HeldItemMesh->AttachToComponent(CameraComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
		UE_LOG(LogTemp, Warning, TEXT("HeldItemComponent: Flatscreen mode - attached to camera"));
	}
	HeldItemMesh->RegisterComponent();

	// Set locked held-item pose (scale comes from item data)
	ApplyHeldItemPose();

	// Start hidden until an item is selected
	HeldItemMesh->SetVisibility(false);
}

void UHeldItemComponent::ApplyHeldItemPose()
{
	if (!HeldItemMesh)
	{
		return;
	}

	HeldItemMesh->SetRelativeLocation(HeldItemOffset);
	HeldItemMesh->SetRelativeRotation(HeldItemRotation);
}

void UHeldItemComponent::UpdateHeldItem(const FName& ItemID)
{
	if (!HeldItemMesh || ItemID.IsNone() || !InventoryComponent) return;

	// Get stored visual data for this item
	FInventoryItemData ItemData = InventoryComponent->GetItemData(ItemID);

	if (ItemData.IsValid())
	{
		// Use stored mesh
		HeldItemMesh->SetStaticMesh(ItemData.Mesh);

		// Apply stored materials
		for (int32 i = 0; i < ItemData.Materials.Num(); i++)
		{
			if (ItemData.Materials[i])
			{
				HeldItemMesh->SetMaterial(i, ItemData.Materials[i]);
			}
		}

		// Apply stored scale
		HeldItemMesh->SetRelativeScale3D(ItemData.Scale);

		UE_LOG(LogTemp, Log, TEXT("HeldItemComponent: Updated to item %s with stored visual data"), *ItemID.ToString());
	}
	else
	{
		// Fallback for items without visual data - use a placeholder cube
		UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (CubeMesh)
		{
			HeldItemMesh->SetStaticMesh(CubeMesh);
			HeldItemMesh->SetRelativeScale3D(FVector(0.1f, 0.75f, 1.0f)); // VHS-like proportions
		}

		// Placeholder material with color based on item name
		UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
		if (BaseMat)
		{
			UMaterialInstanceDynamic* PlaceholderMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (PlaceholderMat)
			{
				uint32 Hash = GetTypeHash(ItemID);
				float R = FMath::Max(((Hash >> 0) & 0xFF) / 255.0f, 0.2f);
				float G = FMath::Max(((Hash >> 8) & 0xFF) / 255.0f, 0.2f);
				float B = FMath::Max(((Hash >> 16) & 0xFF) / 255.0f, 0.2f);
				PlaceholderMat->SetVectorParameterValue(FName("BaseColor"), FLinearColor(R, G, B, 1.0f));
				HeldItemMesh->SetMaterial(0, PlaceholderMat);
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("HeldItemComponent: No visual data for %s, using placeholder"), *ItemID.ToString());
	}
}

void UHeldItemComponent::ShowHeldItem()
{
	if (HeldItemMesh)
	{
		HeldItemMesh->SetVisibility(true);
		FVector WorldLoc = HeldItemMesh->GetComponentLocation();
		UE_LOG(LogTemp, Warning, TEXT("HeldItemComponent: Showing held item %s at world pos %s"), *CurrentItemID.ToString(), *WorldLoc.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HeldItemComponent: ShowHeldItem called but HeldItemMesh is NULL!"));
	}
}

void UHeldItemComponent::HideHeldItem()
{
	if (HeldItemMesh)
	{
		HeldItemMesh->SetVisibility(false);
		UE_LOG(LogTemp, Log, TEXT("HeldItemComponent: Hiding held item"));
	}
}

bool UHeldItemComponent::IsHeldItemVisible() const
{
	return HeldItemMesh && HeldItemMesh->IsVisible();
}

FTransform UHeldItemComponent::GetHeldItemWorldTransform() const
{
	if (HeldItemMesh)
	{
		return HeldItemMesh->GetComponentTransform();
	}
	return FTransform::Identity;
}

void UHeldItemComponent::OnActiveItemChanged(const FName& NewActiveItem)
{
	CurrentItemID = NewActiveItem;

	if (NewActiveItem.IsNone())
	{
		// Active item cleared - hide the held item
		HideHeldItem();
	}
	else
	{
		// Update material immediately (visibility will be handled by tick when inventory closes)
		UpdateHeldItem(NewActiveItem);

		// If inventory is already closed, show immediately
		if (InventoryUIComponent && !InventoryUIComponent->IsInventoryOpen())
		{
			ShowHeldItem();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("HeldItemComponent: Active item changed to %s"), *NewActiveItem.ToString());
}
