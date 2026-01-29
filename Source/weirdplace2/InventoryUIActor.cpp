#include "InventoryUIActor.h"
#include "Inventory.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

AInventoryUIActor::AInventoryUIActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	// Create item name text
	ItemNameText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ItemNameText"));
	ItemNameText->SetupAttachment(RootSceneComponent);
	ItemNameText->SetRelativeLocation(FVector(0.0f, 0.0f, -15.0f));
	ItemNameText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	ItemNameText->SetWorldSize(3.0f);
	ItemNameText->SetTextRenderColor(FColor::White);
	ItemNameText->SetHorizontalAlignment(EHTA_Center);
	ItemNameText->SetVerticalAlignment(EVRTA_TextCenter);
	ItemNameText->SetText(FText::GetEmpty());

	// Load plane mesh for thumbnails
	PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
}

void AInventoryUIActor::BeginPlay()
{
	Super::BeginPlay();

	// Don't create thumbnails here - wait for SetInventoryComponent to be called
}

void AInventoryUIActor::SetInventoryComponent(UInventoryComponent* InInventoryComponent)
{
	InventoryComponent = InInventoryComponent;
	UE_LOG(LogTemp, Log, TEXT("InventoryUIActor: SetInventoryComponent called, InventoryComponent=%p, PlaneMesh=%p"),
		InventoryComponent, PlaneMesh);
}

void AInventoryUIActor::SetSelectedIndex(int32 Index)
{
	SelectedIndex = Index;
	UpdateSelectionHighlight();
}

void AInventoryUIActor::SetGridColumns(int32 Columns)
{
	GridColumns = FMath::Max(1, Columns);
}

void AInventoryUIActor::RefreshDisplay()
{
	UE_LOG(LogTemp, Log, TEXT("InventoryUIActor::RefreshDisplay called, InventoryComponent=%p, PlaneMesh=%p"),
		InventoryComponent, PlaneMesh);
	ClearThumbnails();
	CreateThumbnails();
}

void AInventoryUIActor::SetOpacity(float Opacity)
{
	CurrentOpacity = Opacity;

	// Update opacity on all thumbnail meshes
	for (UStaticMeshComponent* Mesh : ThumbnailMeshes)
	{
		if (Mesh)
		{
			// Create dynamic material if needed and set opacity
			UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
			if (DynMat)
			{
				DynMat->SetScalarParameterValue(FName("Opacity"), Opacity);
			}
		}
	}

	// Update selection highlight opacity
	if (SelectionHighlight)
	{
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(SelectionHighlight->GetMaterial(0));
		if (DynMat)
		{
			DynMat->SetScalarParameterValue(FName("Opacity"), Opacity);
		}
	}

	// Update text opacity
	if (ItemNameText)
	{
		FColor TextColor = FColor::White;
		TextColor.A = FMath::Clamp(static_cast<int32>(Opacity * 255), 0, 255);
		ItemNameText->SetTextRenderColor(TextColor);
	}
}

void AInventoryUIActor::CreateThumbnails()
{
	if (!InventoryComponent || !PlaneMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryUIActor: Missing InventoryComponent or PlaneMesh"));
		return;
	}

	TArray<FName> Items = InventoryComponent->GetItems();

	if (Items.Num() == 0)
	{
		// Show "Empty" text
		if (ItemNameText)
		{
			ItemNameText->SetText(FText::FromString(TEXT("Inventory Empty")));
		}
		return;
	}

	// Create selection highlight first (behind items)
	if (!SelectionHighlight)
	{
		SelectionHighlight = NewObject<UStaticMeshComponent>(this);
		SelectionHighlight->SetStaticMesh(PlaneMesh);
		SelectionHighlight->SetupAttachment(RootSceneComponent);
		SelectionHighlight->RegisterComponent();
		SelectionHighlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// VHS aspect ratio with padding for highlight
		float HighlightWidth = ThumbnailSize * 1.15f;
		float HighlightHeight = ThumbnailSize * 1.4f * 1.15f;
		SelectionHighlight->SetRelativeScale3D(FVector(HighlightHeight * 0.01f, HighlightWidth * 0.01f, 1.0f));

		// Golden/yellow highlight color
		UMaterialInstanceDynamic* HighlightMat = UMaterialInstanceDynamic::Create(
			LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial")), this);
		if (HighlightMat)
		{
			HighlightMat->SetVectorParameterValue(FName("BaseColor"), FLinearColor(1.0f, 0.8f, 0.0f, 1.0f));
			SelectionHighlight->SetMaterial(0, HighlightMat);
		}
	}

	// Create thumbnail for each item
	for (int32 i = 0; i < Items.Num(); i++)
	{
		const FName& ItemID = Items[i];

		UStaticMeshComponent* Thumbnail = NewObject<UStaticMeshComponent>(this);
		Thumbnail->SetStaticMesh(PlaneMesh);
		Thumbnail->SetupAttachment(RootSceneComponent);
		Thumbnail->RegisterComponent();
		Thumbnail->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Position in grid
		FVector Position = CalculateItemPosition(i);
		Thumbnail->SetRelativeLocation(Position);

		// Rotate to face camera (plane faces +Z by default, we want it facing -X toward camera)
		Thumbnail->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		// Scale for VHS aspect ratio (taller than wide)
		float Width = ThumbnailSize;
		float Height = ThumbnailSize * 1.4f;
		Thumbnail->SetRelativeScale3D(FVector(Height * 0.01f, Width * 0.01f, 1.0f));

		// Try to load cover material
		FString MaterialPath = FString::Printf(TEXT("/Game/CreatedMaterials/VHSCoverMaterials/MI_VHSCover_%s"), *ItemID.ToString());
		UMaterialInterface* CoverMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);

		if (CoverMaterial)
		{
			Thumbnail->SetMaterial(0, CoverMaterial);
			UE_LOG(LogTemp, Log, TEXT("Loaded cover material for %s"), *ItemID.ToString());
		}
		else
		{
			// Create a placeholder colored material
			UMaterialInstanceDynamic* PlaceholderMat = UMaterialInstanceDynamic::Create(
				LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial")), this);
			if (PlaceholderMat)
			{
				// Hash the item name to get a unique color
				uint32 Hash = GetTypeHash(ItemID);
				float R = ((Hash >> 0) & 0xFF) / 255.0f;
				float G = ((Hash >> 8) & 0xFF) / 255.0f;
				float B = ((Hash >> 16) & 0xFF) / 255.0f;
				PlaceholderMat->SetVectorParameterValue(FName("BaseColor"), FLinearColor(R, G, B, 1.0f));
				Thumbnail->SetMaterial(0, PlaceholderMat);
			}
			UE_LOG(LogTemp, Warning, TEXT("No cover material found for %s, using placeholder"), *ItemID.ToString());
		}

		ThumbnailMeshes.Add(Thumbnail);
	}

	// Update selection highlight position
	UpdateSelectionHighlight();

	UE_LOG(LogTemp, Log, TEXT("Created %d inventory thumbnails"), Items.Num());
}

void AInventoryUIActor::ClearThumbnails()
{
	for (UStaticMeshComponent* Mesh : ThumbnailMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	ThumbnailMeshes.Empty();

	if (SelectionHighlight)
	{
		SelectionHighlight->DestroyComponent();
		SelectionHighlight = nullptr;
	}
}

void AInventoryUIActor::UpdateSelectionHighlight()
{
	if (!SelectionHighlight || !InventoryComponent) return;

	TArray<FName> Items = InventoryComponent->GetItems();

	if (Items.Num() == 0 || !Items.IsValidIndex(SelectedIndex))
	{
		SelectionHighlight->SetVisibility(false);
		if (ItemNameText)
		{
			ItemNameText->SetText(FText::GetEmpty());
		}
		return;
	}

	// Position highlight behind selected item
	FVector Position = CalculateItemPosition(SelectedIndex);
	Position.X += 0.5f; // Slightly behind the thumbnail
	SelectionHighlight->SetRelativeLocation(Position);
	SelectionHighlight->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	SelectionHighlight->SetVisibility(true);

	// Update item name text
	if (ItemNameText)
	{
		FString ItemName = Items[SelectedIndex].ToString();
		// Make it more readable (replace underscores with spaces)
		ItemName = ItemName.Replace(TEXT("_"), TEXT(" "));
		ItemNameText->SetText(FText::FromString(ItemName));
	}
}

FVector AInventoryUIActor::CalculateItemPosition(int32 Index) const
{
	int32 Row = Index / GridColumns;
	int32 Col = Index % GridColumns;

	// Calculate total grid width to center it
	int32 TotalItems = InventoryComponent ? InventoryComponent->GetItemCount() : 0;
	int32 NumRows = (TotalItems + GridColumns - 1) / GridColumns;
	int32 ItemsInThisRow = FMath::Min(GridColumns, TotalItems - Row * GridColumns);

	float RowWidth = (ItemsInThisRow - 1) * (ThumbnailSize + ThumbnailSpacing);
	float RowStartY = -RowWidth * 0.5f;

	float TotalHeight = (NumRows - 1) * (ThumbnailSize * 1.4f + ThumbnailSpacing);
	float ColStartZ = TotalHeight * 0.5f;

	float Y = RowStartY + Col * (ThumbnailSize + ThumbnailSpacing);
	float Z = ColStartZ - Row * (ThumbnailSize * 1.4f + ThumbnailSpacing);

	return FVector(0.0f, Y, Z);
}
