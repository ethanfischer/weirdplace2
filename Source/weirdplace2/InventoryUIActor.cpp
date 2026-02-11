#include "InventoryUIActor.h"
#include "Inventory.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AInventoryUIActor::AInventoryUIActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	// Load plane mesh for all visuals (using ConstructorHelpers for safe CDO construction)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshAsset.Succeeded())
	{
		PlaneMesh = PlaneMeshAsset.Object;
	}

	// Create background panel
	BackgroundPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackgroundPanel"));
	BackgroundPanel->SetupAttachment(RootSceneComponent);
	if (PlaneMesh)
	{
		BackgroundPanel->SetStaticMesh(PlaneMesh);
	}
	BackgroundPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackgroundPanel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	BackgroundPanel->SetRelativeLocation(FVector(1.0f, 0.0f, 0.0f)); // Slightly behind everything

	// Create item name text
	ItemNameText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ItemNameText"));
	ItemNameText->SetupAttachment(RootSceneComponent);
	ItemNameText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	ItemNameText->SetWorldSize(3.0f);
	ItemNameText->SetTextRenderColor(FColor::White);
	ItemNameText->SetHorizontalAlignment(EHTA_Left);
	ItemNameText->SetVerticalAlignment(EVRTA_TextCenter);
	ItemNameText->SetText(FText::GetEmpty());

	// Create item counter text
	ItemCounterText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ItemCounterText"));
	ItemCounterText->SetupAttachment(RootSceneComponent);
	ItemCounterText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	ItemCounterText->SetWorldSize(2.5f);
	ItemCounterText->SetTextRenderColor(FColor(180, 180, 180));
	ItemCounterText->SetHorizontalAlignment(EHTA_Right);
	ItemCounterText->SetVerticalAlignment(EVRTA_TextCenter);
	ItemCounterText->SetText(FText::GetEmpty());
}

void AInventoryUIActor::BeginPlay()
{
	Super::BeginPlay();

	// Create dynamic material for background
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	if (BaseMat && BackgroundPanel)
	{
		BackgroundMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (BackgroundMaterial)
		{
			BackgroundMaterial->SetVectorParameterValue(FName("BaseColor"), BackgroundColor);
			BackgroundPanel->SetMaterial(0, BackgroundMaterial);
		}
	}
}

void AInventoryUIActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateHoverAnimation(DeltaTime);
}

void AInventoryUIActor::SetInventoryComponent(UInventoryComponent* InInventoryComponent)
{
	InventoryComponent = InInventoryComponent;
}

void AInventoryUIActor::SetSelectedIndex(int32 Index)
{
	int32 NewIndex = FMath::Clamp(Index, 0, GetTotalSlots() - 1);
	if (NewIndex != SelectedIndex)
	{
		PreviousSelectedIndex = SelectedIndex;
		SelectedIndex = NewIndex;
		HoverAnimationProgress = 0.0f; // Reset animation when selection changes
		UpdateSelectionHighlight();
	}
}

void AInventoryUIActor::SetGridColumns(int32 Columns)
{
	GridColumns = FMath::Max(1, Columns);
}

void AInventoryUIActor::SetGridRows(int32 Rows)
{
	GridRows = FMath::Max(1, Rows);
}

void AInventoryUIActor::RefreshDisplay()
{
	ClearSlots();
	ClearThumbnails();
	CreateSlots();
	CreateThumbnails();
	UpdateBackgroundSize();
	UpdateSelectionHighlight();
	UpdateItemCounter();
}

void AInventoryUIActor::SetOpacity(float Opacity)
{
	CurrentOpacity = Opacity;

	// Update background opacity
	if (BackgroundMaterial)
	{
		FLinearColor AdjustedColor = BackgroundColor;
		AdjustedColor.A *= Opacity;
		BackgroundMaterial->SetVectorParameterValue(FName("BaseColor"), AdjustedColor);
	}

	// Update slot meshes opacity
	for (UStaticMeshComponent* Mesh : SlotMeshes)
	{
		if (Mesh)
		{
			UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
			if (DynMat)
			{
				DynMat->SetScalarParameterValue(FName("Opacity"), Opacity);
			}
		}
	}

	// Update thumbnail meshes opacity
	for (UStaticMeshComponent* Mesh : ThumbnailMeshes)
	{
		if (Mesh)
		{
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

	// Update active item border opacity
	if (ActiveItemBorder)
	{
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(ActiveItemBorder->GetMaterial(0));
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

	if (ItemCounterText)
	{
		FColor TextColor = FColor(180, 180, 180);
		TextColor.A = FMath::Clamp(static_cast<int32>(Opacity * 255), 0, 255);
		ItemCounterText->SetTextRenderColor(TextColor);
	}
}

void AInventoryUIActor::CreateSlots()
{
	if (!PlaneMesh) return;

	int32 TotalSlots = GetTotalSlots();
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));

	for (int32 i = 0; i < TotalSlots; i++)
	{
		// Create slot background
		UStaticMeshComponent* SlotMesh = NewObject<UStaticMeshComponent>(this);
		SlotMesh->SetStaticMesh(PlaneMesh);
		SlotMesh->SetupAttachment(RootSceneComponent);
		SlotMesh->RegisterComponent();
		SlotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Position in grid
		FVector Position = CalculateSlotPosition(i);
		Position.X += 0.3f; // Slightly behind thumbnails
		SlotMesh->SetRelativeLocation(Position);
		SlotMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		// Scale for slot size (slightly larger than thumbnail for border effect)
		float Width = ThumbnailSize * 1.05f;
		float Height = ThumbnailSize * 1.4f * 1.05f;
		SlotMesh->SetRelativeScale3D(FVector(Height * 0.01f, Width * 0.01f, 1.0f));

		// Create empty slot material
		if (BaseMat)
		{
			UMaterialInstanceDynamic* SlotMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (SlotMat)
			{
				SlotMat->SetVectorParameterValue(FName("BaseColor"), EmptySlotColor);
				SlotMesh->SetMaterial(0, SlotMat);
			}
		}

		SlotMeshes.Add(SlotMesh);
	}

	// Create selection highlight
	if (!SelectionHighlight)
	{
		SelectionHighlight = NewObject<UStaticMeshComponent>(this);
		SelectionHighlight->SetStaticMesh(PlaneMesh);
		SelectionHighlight->SetupAttachment(RootSceneComponent);
		SelectionHighlight->RegisterComponent();
		SelectionHighlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Slightly larger than slot for highlight border
		float HighlightWidth = ThumbnailSize * 1.15f;
		float HighlightHeight = ThumbnailSize * 1.4f * 1.15f;
		SelectionHighlight->SetRelativeScale3D(FVector(HighlightHeight * 0.01f, HighlightWidth * 0.01f, 1.0f));
		SelectionHighlight->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		if (BaseMat)
		{
			SelectionMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (SelectionMaterial)
			{
				SelectionMaterial->SetVectorParameterValue(FName("BaseColor"), SelectionColor);
				SelectionHighlight->SetMaterial(0, SelectionMaterial);
			}
		}
	}

	// Create active item border (shows which item is confirmed/equipped)
	if (!ActiveItemBorder)
	{
		ActiveItemBorder = NewObject<UStaticMeshComponent>(this);
		ActiveItemBorder->SetStaticMesh(PlaneMesh);
		ActiveItemBorder->SetupAttachment(RootSceneComponent);
		ActiveItemBorder->RegisterComponent();
		ActiveItemBorder->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Larger than thumbnail to create a visible border frame
		float BorderWidth = ThumbnailSize * 1.2f;
		float BorderHeight = ThumbnailSize * 1.4f * 1.2f;
		ActiveItemBorder->SetRelativeScale3D(FVector(BorderHeight * 0.01f, BorderWidth * 0.01f, 1.0f));
		ActiveItemBorder->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		if (BaseMat)
		{
			UMaterialInstanceDynamic* ActiveMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (ActiveMat)
			{
				ActiveMat->SetVectorParameterValue(FName("BaseColor"), ActiveItemColor);
				ActiveItemBorder->SetMaterial(0, ActiveMat);
			}
		}

		// Start hidden until an item is selected
		ActiveItemBorder->SetVisibility(false);
		UE_LOG(LogTemp, Warning, TEXT("Created ActiveItemBorder"));
	}

	// Reset hover animation state
	HoverAnimationProgress = 0.0f;
	PulseTime = 0.0f;
}

void AInventoryUIActor::ClearSlots()
{
	for (UStaticMeshComponent* Mesh : SlotMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	SlotMeshes.Empty();

	if (SelectionHighlight)
	{
		SelectionHighlight->DestroyComponent();
		SelectionHighlight = nullptr;
	}

	if (ActiveItemBorder)
	{
		ActiveItemBorder->DestroyComponent();
		ActiveItemBorder = nullptr;
	}
}

void AInventoryUIActor::CreateThumbnails()
{
	if (!InventoryComponent || !PlaneMesh) return;

	TArray<FName> Items = InventoryComponent->GetItems();

	// Create thumbnail for each collected item
	for (int32 i = 0; i < Items.Num() && i < GetTotalSlots(); i++)
	{
		const FName& ItemID = Items[i];

		UStaticMeshComponent* Thumbnail = NewObject<UStaticMeshComponent>(this);
		Thumbnail->SetStaticMesh(PlaneMesh);
		Thumbnail->SetupAttachment(RootSceneComponent);
		Thumbnail->RegisterComponent();
		Thumbnail->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Position in grid (on top of slot)
		FVector Position = CalculateSlotPosition(i);
		Thumbnail->SetRelativeLocation(Position);
		Thumbnail->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		// Scale for thumbnail size
		float Width = ThumbnailSize;
		float Height = ThumbnailSize * 1.4f;
		Thumbnail->SetRelativeScale3D(FVector(Height * 0.01f, Width * 0.01f, 1.0f));

		// Try to load cover material
		FString MaterialPath = FString::Printf(TEXT("/Game/CreatedMaterials/VHSCoverMaterials/MI_VHSCover_%s"), *ItemID.ToString());
		UMaterialInterface* CoverMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);

		if (CoverMaterial)
		{
			Thumbnail->SetMaterial(0, CoverMaterial);
		}
		else
		{
			// Create a placeholder colored material
			UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
			UMaterialInstanceDynamic* PlaceholderMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (PlaceholderMat)
			{
				// Hash the item name to get a unique color
				uint32 Hash = GetTypeHash(ItemID);
				float R = ((Hash >> 0) & 0xFF) / 255.0f;
				float G = ((Hash >> 8) & 0xFF) / 255.0f;
				float B = ((Hash >> 16) & 0xFF) / 255.0f;
				// Ensure minimum brightness
				R = FMath::Max(R, 0.2f);
				G = FMath::Max(G, 0.2f);
				B = FMath::Max(B, 0.2f);
				PlaceholderMat->SetVectorParameterValue(FName("BaseColor"), FLinearColor(R, G, B, 1.0f));
				Thumbnail->SetMaterial(0, PlaceholderMat);
			}
		}

		ThumbnailMeshes.Add(Thumbnail);
	}
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
}

void AInventoryUIActor::UpdateSelectionHighlight()
{
	if (!SelectionHighlight) return;

	// Always show selection highlight (even on empty slots)
	FVector Position = CalculateSlotPosition(SelectedIndex);
	Position.X += 0.5f; // Behind the slot
	SelectionHighlight->SetRelativeLocation(Position);
	SelectionHighlight->SetVisibility(true);

	// Note: Item name text is now updated via SetActiveItemName when user confirms selection
}

void AInventoryUIActor::SetActiveItem(const FName& ItemID, int32 ItemIndex)
{
	// Update text
	if (ItemNameText)
	{
		if (ItemID.IsNone())
		{
			ItemNameText->SetText(FText::FromString(TEXT("No Item Selected")));
		}
		else
		{
			FString ItemName = ItemID.ToString();
			ItemName = ItemName.Replace(TEXT("_"), TEXT(" "));
			ItemName = ItemName.Replace(TEXT("-"), TEXT(" "));
			ItemNameText->SetText(FText::FromString(ItemName));
		}
	}

	// Update active item border
	ActiveItemIndex = ItemIndex;
	if (ActiveItemBorder)
	{
		if (ItemID.IsNone() || ItemIndex < 0)
		{
			ActiveItemBorder->SetVisibility(false);
			UE_LOG(LogTemp, Log, TEXT("ActiveItemBorder hidden (no item)"));
		}
		else
		{
			FVector Position = CalculateSlotPosition(ItemIndex);
			Position.X += 0.1f; // Slightly behind thumbnail so it frames it
			ActiveItemBorder->SetRelativeLocation(Position);
			ActiveItemBorder->SetVisibility(true);
			UE_LOG(LogTemp, Warning, TEXT("ActiveItemBorder shown at index %d, pos %s"), ItemIndex, *Position.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ActiveItemBorder is NULL!"));
	}
}

void AInventoryUIActor::UpdateBackgroundSize()
{
	if (!BackgroundPanel) return;

	float GridW = GetGridWidth();
	float GridH = GetGridHeight();

	// Add padding
	float TotalWidth = GridW + BackgroundPadding * 2.0f;
	float TotalHeight = GridH + BackgroundPadding * 2.0f + 8.0f; // Extra space for text at bottom

	// Scale background (plane is 100x100 units by default)
	BackgroundPanel->SetRelativeScale3D(FVector(TotalHeight * 0.01f, TotalWidth * 0.01f, 1.0f));

	// Position text at bottom of grid
	float TextY = -GridW * 0.5f + 2.0f;
	float TextZ = -GridH * 0.5f - BackgroundPadding - 2.0f;

	if (ItemNameText)
	{
		ItemNameText->SetRelativeLocation(FVector(0.0f, TextY, TextZ));
	}

	if (ItemCounterText)
	{
		ItemCounterText->SetRelativeLocation(FVector(0.0f, -TextY, TextZ));
	}
}

void AInventoryUIActor::UpdateItemCounter()
{
	if (!ItemCounterText) return;

	int32 CollectedCount = InventoryComponent ? InventoryComponent->GetItemCount() : 0;
	int32 TotalSlots = GetTotalSlots();

	FString CounterText = FString::Printf(TEXT("%d/%d"), CollectedCount, TotalSlots);
	ItemCounterText->SetText(FText::FromString(CounterText));
}

FVector AInventoryUIActor::CalculateSlotPosition(int32 Index) const
{
	int32 Row = Index / GridColumns;
	int32 Col = Index % GridColumns;

	float SlotWidth = ThumbnailSize + ThumbnailSpacing;
	float SlotHeight = ThumbnailSize * 1.4f + ThumbnailSpacing;

	// Center the grid
	float TotalWidth = (GridColumns - 1) * SlotWidth;
	float TotalHeight = (GridRows - 1) * SlotHeight;

	float Y = -TotalWidth * 0.5f + Col * SlotWidth;
	float Z = TotalHeight * 0.5f - Row * SlotHeight;

	return FVector(0.0f, Y, Z);
}

float AInventoryUIActor::GetGridWidth() const
{
	return GridColumns * ThumbnailSize + (GridColumns - 1) * ThumbnailSpacing;
}

float AInventoryUIActor::GetGridHeight() const
{
	return GridRows * (ThumbnailSize * 1.4f) + (GridRows - 1) * ThumbnailSpacing;
}

void AInventoryUIActor::UpdateHoverAnimation(float DeltaTime)
{
	// Animate hover scale
	HoverAnimationProgress = FMath::FInterpTo(HoverAnimationProgress, 1.0f, DeltaTime, HoverAnimationSpeed);

	// Update pulse time
	PulseTime += DeltaTime * SelectionPulseSpeed;

	// Calculate pulse value (oscillates between 0 and 1)
	float PulseValue = (FMath::Sin(PulseTime * 2.0f * PI) + 1.0f) * 0.5f;

	// Apply scale to selected slot/thumbnail
	float CurrentScale = FMath::Lerp(1.0f, HoverScaleMultiplier, HoverAnimationProgress);

	// Scale the selected slot
	if (SlotMeshes.IsValidIndex(SelectedIndex))
	{
		UStaticMeshComponent* SelectedSlot = SlotMeshes[SelectedIndex];
		if (SelectedSlot)
		{
			float Width = ThumbnailSize * 1.05f * CurrentScale;
			float Height = ThumbnailSize * 1.4f * 1.05f * CurrentScale;
			SelectedSlot->SetRelativeScale3D(FVector(Height * 0.01f, Width * 0.01f, 1.0f));
		}
	}

	// Scale the selected thumbnail if it exists
	if (ThumbnailMeshes.IsValidIndex(SelectedIndex))
	{
		UStaticMeshComponent* SelectedThumbnail = ThumbnailMeshes[SelectedIndex];
		if (SelectedThumbnail)
		{
			float Width = ThumbnailSize * CurrentScale;
			float Height = ThumbnailSize * 1.4f * CurrentScale;
			SelectedThumbnail->SetRelativeScale3D(FVector(Height * 0.01f, Width * 0.01f, 1.0f));
		}
	}

	// Reset scale on previously selected slot
	if (PreviousSelectedIndex >= 0 && PreviousSelectedIndex != SelectedIndex)
	{
		if (SlotMeshes.IsValidIndex(PreviousSelectedIndex))
		{
			UStaticMeshComponent* PrevSlot = SlotMeshes[PreviousSelectedIndex];
			if (PrevSlot)
			{
				float Width = ThumbnailSize * 1.05f;
				float Height = ThumbnailSize * 1.4f * 1.05f;
				PrevSlot->SetRelativeScale3D(FVector(Height * 0.01f, Width * 0.01f, 1.0f));
			}
		}

		if (ThumbnailMeshes.IsValidIndex(PreviousSelectedIndex))
		{
			UStaticMeshComponent* PrevThumbnail = ThumbnailMeshes[PreviousSelectedIndex];
			if (PrevThumbnail)
			{
				float Width = ThumbnailSize;
				float Height = ThumbnailSize * 1.4f;
				PrevThumbnail->SetRelativeScale3D(FVector(Height * 0.01f, Width * 0.01f, 1.0f));
			}
		}

		PreviousSelectedIndex = -1; // Clear after resetting
	}

	// Pulse the selection highlight
	if (SelectionHighlight && SelectionMaterial)
	{
		// Scale the highlight with hover + pulse
		float HighlightScale = HoverScaleMultiplier + PulseValue * SelectionPulseIntensity;
		float HighlightWidth = ThumbnailSize * HighlightScale;
		float HighlightHeight = ThumbnailSize * 1.4f * HighlightScale;
		SelectionHighlight->SetRelativeScale3D(FVector(HighlightHeight * 0.01f, HighlightWidth * 0.01f, 1.0f));

		// Pulse the color brightness
		float ColorIntensity = 1.0f + PulseValue * SelectionPulseIntensity;
		FLinearColor PulsedColor = SelectionColor * ColorIntensity;
		PulsedColor.A = SelectionColor.A * CurrentOpacity;
		SelectionMaterial->SetVectorParameterValue(FName("BaseColor"), PulsedColor);
	}
}
