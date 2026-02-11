#include "HeldItemComponent.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "MyCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

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
			UpdateHeldItemMaterial(CurrentItemID);
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
	if (!Owner || !CameraComponent) return;

	// Load the plane mesh (same as InventoryUIActor uses)
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!PlaneMesh) return;

	// Create the mesh component
	HeldItemMesh = NewObject<UStaticMeshComponent>(Owner);
	if (!HeldItemMesh) return;

	HeldItemMesh->SetStaticMesh(PlaneMesh);
	HeldItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldItemMesh->SetCastShadow(false);

	// Attach to camera
	HeldItemMesh->AttachToComponent(CameraComponent, FAttachmentTransformRules::KeepRelativeTransform);
	HeldItemMesh->RegisterComponent();

	// Set relative transform
	HeldItemMesh->SetRelativeLocation(HeldItemOffset);
	HeldItemMesh->SetRelativeRotation(HeldItemRotation);
	HeldItemMesh->SetRelativeScale3D(HeldItemScale * 0.01f); // Scale down (plane is 100x100 units)

	// Start hidden
	HeldItemMesh->SetVisibility(false);
}

void UHeldItemComponent::UpdateHeldItemMaterial(const FName& ItemID)
{
	if (!HeldItemMesh || ItemID.IsNone()) return;

	// Try to load cover material (same path as InventoryUIActor)
	FString MaterialPath = FString::Printf(TEXT("/Game/CreatedMaterials/VHSCoverMaterials/MI_VHSCover_%s"), *ItemID.ToString());
	UMaterialInterface* CoverMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);

	if (CoverMaterial)
	{
		HeldItemMesh->SetMaterial(0, CoverMaterial);
	}
	else
	{
		// Fallback to default material with a color based on item name
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
		UE_LOG(LogTemp, Warning, TEXT("HeldItemComponent: Could not load material for %s, using placeholder"), *ItemID.ToString());
	}
}

void UHeldItemComponent::ShowHeldItem()
{
	if (HeldItemMesh)
	{
		HeldItemMesh->SetVisibility(true);
		UE_LOG(LogTemp, Log, TEXT("HeldItemComponent: Showing held item %s"), *CurrentItemID.ToString());
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
		UpdateHeldItemMaterial(NewActiveItem);

		// If inventory is already closed, show immediately
		if (InventoryUIComponent && !InventoryUIComponent->IsInventoryOpen())
		{
			ShowHeldItem();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("HeldItemComponent: Active item changed to %s"), *NewActiveItem.ToString());
}
