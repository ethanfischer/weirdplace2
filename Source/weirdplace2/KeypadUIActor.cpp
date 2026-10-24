#include "KeypadUIActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AKeypadUIActor::AKeypadUIActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	// Plane mesh for all visuals (safe CDO construction via ConstructorHelpers).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshAsset.Succeeded())
	{
		PlaneMesh = PlaneMeshAsset.Object;
	}

	// Same translucent pane as the inventory strip and pause menu.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PanelAsset(TEXT("/Game/CreatedMaterials/M_ShadowGlass.M_ShadowGlass"));
	UMaterialInterface* DefaultPanelMat = PanelAsset.Succeeded() ? PanelAsset.Object : nullptr;

	BackgroundPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackgroundPanel"));
	BackgroundPanel->SetupAttachment(RootSceneComponent);
	if (PlaneMesh)
	{
		BackgroundPanel->SetStaticMesh(PlaneMesh);
	}
	if (DefaultPanelMat)
	{
		BackgroundPanel->SetMaterial(0, DefaultPanelMat);
	}
	BackgroundPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackgroundPanel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	BackgroundPanel->SetRelativeLocation(FVector(1.0f, 0.0f, 0.0f)); // Behind everything
}

void AKeypadUIActor::BeginPlay()
{
	Super::BeginPlay();

	// Self-illuminated UI: the engine default TextRender material is lit and would
	// render near-black, so the dynamic-material setup happens here. (Digit/fill text
	// get their unlit material in ApplyUnlitTextMaterial after the texts exist.)
	if (BackgroundPanel)
	{
		UMaterialInterface* BaseMat = BackgroundPanel->GetMaterial(0);
		if (BaseMat)
		{
			BackgroundMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
			BackgroundPanel->SetMaterial(0, BackgroundMaterial);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("KeypadUIActor: BackgroundPanel has no material; panel will not render."));
		}
	}
}

void AKeypadUIActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateHoverAnimation(DeltaTime);
}

void AKeypadUIActor::BuildKeypad()
{
	CreateCells();
	CreateFillSlots();
	UpdateBackgroundSize();
	UpdateSelectionHighlight();
	RefreshCellColors();
	ApplyUnlitTextMaterial();
}

void AKeypadUIActor::RefreshCellColors()
{
	for (int32 i = 0; i < CellMaterials.Num(); i++)
	{
		if (UMaterialInstanceDynamic* Mat = CellMaterials[i])
		{
			const FLinearColor Color = (i == SelectedIndex) ? SelectionColor : CellColor;
			Mat->SetVectorParameterValue(FName("Color"), Color);
			Mat->SetVectorParameterValue(FName("BaseColor"), Color);
			Mat->SetVectorParameterValue(FName("EmissiveColor"), Color);
		}
	}
}

UMaterialInterface* AKeypadUIActor::ResolveSlotMaterial() const
{
	if (SlotMaterial)
	{
		return SlotMaterial;
	}
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_SolidColor.M_SolidColor"));
	if (BaseMat)
	{
		UE_LOG(LogTemp, Warning, TEXT("KeypadUIActor: SlotMaterial not assigned. Falling back to /Game/Materials/M_SolidColor."));
		return BaseMat;
	}
	UE_LOG(LogTemp, Warning, TEXT("KeypadUIActor: M_SolidColor missing. Falling back to engine DefaultMaterial for slots."));
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
}

void AKeypadUIActor::CreateCells()
{
	if (!PlaneMesh)
	{
		return;
	}

	UMaterialInterface* BaseMat = ResolveSlotMaterial();

	for (int32 i = 0; i < NumDigits; i++)
	{
		// Cell background plane
		UStaticMeshComponent* CellMesh = NewObject<UStaticMeshComponent>(this);
		CellMesh->SetStaticMesh(PlaneMesh);
		CellMesh->SetupAttachment(RootSceneComponent);
		CellMesh->RegisterComponent();
		CellMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		FVector Position = CalculateCellPosition(i);
		Position.X += 0.3f; // Slightly behind the digit text
		CellMesh->SetRelativeLocation(Position);
		CellMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		const float CellPlane = CellSize * 1.05f;
		CellMesh->SetRelativeScale3D(FVector(CellPlane * 0.01f, CellPlane * 0.01f, 1.0f));
		CellMesh->SetVisibility(false); // digits only; the box read as a gray slab under eye adaptation

		UMaterialInstanceDynamic* CellMat = BaseMat ? UMaterialInstanceDynamic::Create(BaseMat, this) : nullptr;
		if (CellMat)
		{
			CellMat->SetVectorParameterValue(FName("Color"), CellColor);
			CellMat->SetVectorParameterValue(FName("BaseColor"), CellColor);
			CellMat->SetVectorParameterValue(FName("EmissiveColor"), CellColor);
			CellMesh->SetMaterial(0, CellMat);
		}
		CellMeshes.Add(CellMesh);
		CellMaterials.Add(CellMat);

		// Digit label (attached to root, not the rotated plane, so it faces the player)
		UTextRenderComponent* DigitText = NewObject<UTextRenderComponent>(this);
		DigitText->SetupAttachment(RootSceneComponent);
		DigitText->RegisterComponent();
		FVector TextPos = CalculateCellPosition(i);
		TextPos.X -= 0.1f; // In front of the cell
		DigitText->SetRelativeLocation(TextPos);
		DigitText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		DigitText->SetWorldSize(CellSize * 0.6f);
		DigitText->SetTextRenderColor(FColor(230, 219, 255));
		DigitText->SetHorizontalAlignment(EHTA_Center);
		DigitText->SetVerticalAlignment(EVRTA_TextCenter);
		DigitText->SetText(FText::AsNumber(i + 1));
		DigitTexts.Add(DigitText);
	}

	// Selection highlight (created once)
	if (!SelectionHighlight)
	{
		SelectionHighlight = NewObject<UStaticMeshComponent>(this);
		SelectionHighlight->SetStaticMesh(PlaneMesh);
		SelectionHighlight->SetupAttachment(RootSceneComponent);
		SelectionHighlight->RegisterComponent();
		SelectionHighlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Underline: plane is 100x100, local X (after the pitch) is height, Y is width.
		SelectionHighlight->SetRelativeScale3D(FVector(0.35f * 0.01f, CellSize * 0.6f * 0.01f, 1.0f));
		SelectionHighlight->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		UMaterialInterface* HighlightBase = SelectionHighlightMaterial ? SelectionHighlightMaterial : ResolveSlotMaterial();
		if (HighlightBase)
		{
			SelectionMaterial = UMaterialInstanceDynamic::Create(HighlightBase, this);
			if (SelectionMaterial)
			{
				SelectionMaterial->SetVectorParameterValue(FName("Color"), SelectionColor);
				SelectionMaterial->SetVectorParameterValue(FName("BaseColor"), SelectionColor);
				SelectionMaterial->SetVectorParameterValue(FName("EmissiveColor"), SelectionColor);
				SelectionHighlight->SetMaterial(0, SelectionMaterial);
			}
		}
	}

	HoverAnimationProgress = 0.0f;
}

void AKeypadUIActor::CreateFillSlots()
{
	if (!PlaneMesh)
	{
		return;
	}

	// Clear any previous fill row (SetCodeLength can rebuild it).
	for (UStaticMeshComponent* Mesh : FillSlotMeshes)
	{
		if (Mesh) { Mesh->DestroyComponent(); }
	}
	FillSlotMeshes.Empty();
	for (UTextRenderComponent* Text : FillSlotTexts)
	{
		if (Text) { Text->DestroyComponent(); }
	}
	FillSlotTexts.Empty();

	UMaterialInterface* BaseMat = ResolveSlotMaterial();

	for (int32 i = 0; i < CodeLength; i++)
	{
		UStaticMeshComponent* SlotMesh = NewObject<UStaticMeshComponent>(this);
		SlotMesh->SetStaticMesh(PlaneMesh);
		SlotMesh->SetupAttachment(RootSceneComponent);
		SlotMesh->RegisterComponent();
		SlotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		FVector Position = CalculateFillSlotPosition(i);
		Position.X += 0.3f;
		SlotMesh->SetRelativeLocation(Position);
		SlotMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		const float SlotPlane = CellSize * 0.7f;
		SlotMesh->SetRelativeScale3D(FVector(SlotPlane * 0.01f, SlotPlane * 0.01f, 1.0f));
		SlotMesh->SetVisibility(false); // empty slots show "_" instead of a box

		if (BaseMat)
		{
			UMaterialInstanceDynamic* SlotMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (SlotMat)
			{
				SlotMat->SetVectorParameterValue(FName("Color"), EmptySlotColor);
				SlotMat->SetVectorParameterValue(FName("BaseColor"), EmptySlotColor);
				SlotMat->SetVectorParameterValue(FName("EmissiveColor"), EmptySlotColor);
				SlotMesh->SetMaterial(0, SlotMat);
			}
		}
		FillSlotMeshes.Add(SlotMesh);

		UTextRenderComponent* SlotText = NewObject<UTextRenderComponent>(this);
		SlotText->SetupAttachment(RootSceneComponent);
		SlotText->RegisterComponent();
		FVector TextPos = CalculateFillSlotPosition(i);
		TextPos.X -= 0.1f;
		SlotText->SetRelativeLocation(TextPos);
		SlotText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		SlotText->SetWorldSize(CellSize * 0.5f);
		SlotText->SetTextRenderColor(FColor(230, 219, 255));
		SlotText->SetHorizontalAlignment(EHTA_Center);
		SlotText->SetVerticalAlignment(EVRTA_TextCenter);
		SlotText->SetText(FText::FromString(TEXT("_")));
		FillSlotTexts.Add(SlotText);
	}
}

void AKeypadUIActor::ApplyUnlitTextMaterial()
{
	UMaterialInterface* TextMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_UnlitText.M_UnlitText"));
	if (!TextMat)
	{
		UE_LOG(LogTemp, Warning, TEXT("KeypadUIActor: M_UnlitText not found; digit text may be dark."));
		return;
	}
	TArray<UTextRenderComponent*> TextComps;
	GetComponents<UTextRenderComponent>(TextComps);
	for (UTextRenderComponent* TextComp : TextComps)
	{
		TextComp->SetTextMaterial(TextMat);
	}
}

void AKeypadUIActor::SetSelectedCell(int32 CellIndex)
{
	const int32 NewIndex = FMath::Clamp(CellIndex, 0, NumDigits - 1);
	if (NewIndex != SelectedIndex)
	{
		PreviousSelectedIndex = SelectedIndex;
		SelectedIndex = NewIndex;
		HoverAnimationProgress = 0.0f;
		UpdateSelectionHighlight();
		RefreshCellColors();
	}
}

void AKeypadUIActor::SetCodeLength(int32 Len)
{
	const int32 NewLen = FMath::Max(1, Len);
	if (NewLen == CodeLength && FillSlotMeshes.Num() == NewLen)
	{
		return;
	}
	CodeLength = NewLen;
	// Only rebuild if cells already exist (BuildKeypad rebuilds everything anyway).
	if (CellMeshes.Num() > 0)
	{
		CreateFillSlots();
		UpdateBackgroundSize();
		ApplyUnlitTextMaterial();
	}
}

void AKeypadUIActor::SetEnteredCode(const FString& Code)
{
	for (int32 i = 0; i < FillSlotTexts.Num(); i++)
	{
		const bool bFilled = i < Code.Len();
		if (UTextRenderComponent* Text = FillSlotTexts[i])
		{
			Text->SetText(bFilled ? FText::FromString(Code.Mid(i, 1)) : FText::FromString(TEXT("_")));
		}
		if (FillSlotMeshes.IsValidIndex(i))
		{
			if (UMaterialInstanceDynamic* SlotMat = Cast<UMaterialInstanceDynamic>(FillSlotMeshes[i]->GetMaterial(0)))
			{
				const FLinearColor Color = bFilled ? FilledSlotColor : EmptySlotColor;
				SlotMat->SetVectorParameterValue(FName("Color"), Color);
				SlotMat->SetVectorParameterValue(FName("BaseColor"), Color);
				SlotMat->SetVectorParameterValue(FName("EmissiveColor"), Color);
			}
		}
	}
}

void AKeypadUIActor::SetOpacity(float Opacity)
{
	CurrentOpacity = Opacity;

	if (BackgroundMaterial)
	{
		FLinearColor AdjustedColor = BackgroundColor;
		AdjustedColor.A *= Opacity;
		BackgroundMaterial->SetVectorParameterValue(FName("BaseColor"), AdjustedColor);
	}

	const FColor TextColor(230, 219, 255, FMath::Clamp(static_cast<int32>(Opacity * 255), 0, 255));
	for (UTextRenderComponent* Text : DigitTexts)
	{
		if (Text) { Text->SetTextRenderColor(TextColor); }
	}
	for (UTextRenderComponent* Text : FillSlotTexts)
	{
		if (Text) { Text->SetTextRenderColor(TextColor); }
	}
}

void AKeypadUIActor::UpdateSelectionHighlight()
{
	if (!SelectionHighlight)
	{
		return;
	}
	// Just below the grown digit.
	FVector Position = CalculateCellPosition(SelectedIndex);
	Position.Z -= CellSize * 0.6f * 1.12f * 0.5f + 0.6f;
	SelectionHighlight->SetRelativeLocation(Position);
	SelectionHighlight->SetVisibility(true);
}

void AKeypadUIActor::UpdateBackgroundSize()
{
	if (!BackgroundPanel)
	{
		return;
	}

	const float GridW = GetGridWidth();
	const float GridH = GetGridHeight();

	// Reserve room below the grid for the fill-slot row.
	const float FillRowExtent = CellSize * 1.6f;
	const float TotalWidth = FMath::Max(GridW, (CodeLength - 1) * (CellSize * 0.7f + CellSpacing) + CellSize) + BackgroundPadding * 2.0f;
	const float TotalHeight = GridH + FillRowExtent + BackgroundPadding * 2.0f;

	BackgroundPanel->SetRelativeScale3D(FVector(TotalHeight * 0.01f, TotalWidth * 0.01f, 1.0f));
	// Fill row sits above the grid, so shift the panel up to keep grid+fill centered.
	BackgroundPanel->SetRelativeLocation(FVector(1.0f, 0.0f, FillRowExtent * 0.5f));
}

FVector AKeypadUIActor::CalculateCellPosition(int32 Index) const
{
	const int32 Row = Index / GridColumns;
	const int32 Col = Index % GridColumns;

	const float SlotWidth = CellSize + CellSpacing;
	const float SlotHeight = CellSize + CellSpacing;

	const float TotalWidth = (GridColumns - 1) * SlotWidth;
	const float TotalHeight = (GridRows - 1) * SlotHeight;

	const float Y = -TotalWidth * 0.5f + Col * SlotWidth;
	const float Z = TotalHeight * 0.5f - Row * SlotHeight;

	return FVector(0.0f, Y, Z);
}

FVector AKeypadUIActor::CalculateFillSlotPosition(int32 Index) const
{
	const float SlotPitch = CellSize * 0.7f + CellSpacing;
	const float TotalWidth = (CodeLength - 1) * SlotPitch;
	const float Y = -TotalWidth * 0.5f + Index * SlotPitch;

	// One slot-height above the top grid row.
	const float SlotHeight = CellSize + CellSpacing;
	const float TopRowZ = (GridRows - 1) * SlotHeight * 0.5f;
	const float Z = TopRowZ + CellSize * 1.2f;

	return FVector(0.0f, Y, Z);
}

float AKeypadUIActor::GetGridWidth() const
{
	return GridColumns * CellSize + (GridColumns - 1) * CellSpacing;
}

float AKeypadUIActor::GetGridHeight() const
{
	return GridRows * CellSize + (GridRows - 1) * CellSpacing;
}

void AKeypadUIActor::UpdateHoverAnimation(float DeltaTime)
{
	HoverAnimationProgress = FMath::FInterpTo(HoverAnimationProgress, 1.0f, DeltaTime, HoverAnimationSpeed);
	const float CurrentScale = FMath::Lerp(1.0f, HoverScaleMultiplier, HoverAnimationProgress);

	// Scale the selected cell up.
	if (CellMeshes.IsValidIndex(SelectedIndex) && CellMeshes[SelectedIndex])
	{
		const float Plane = CellSize * 1.05f * CurrentScale;
		CellMeshes[SelectedIndex]->SetRelativeScale3D(FVector(Plane * 0.01f, Plane * 0.01f, 1.0f));
	}

	// Reset the previously-selected cell.
	if (PreviousSelectedIndex >= 0 && PreviousSelectedIndex != SelectedIndex)
	{
		if (CellMeshes.IsValidIndex(PreviousSelectedIndex) && CellMeshes[PreviousSelectedIndex])
		{
			const float Plane = CellSize * 1.05f;
			CellMeshes[PreviousSelectedIndex]->SetRelativeScale3D(FVector(Plane * 0.01f, Plane * 0.01f, 1.0f));
		}
		PreviousSelectedIndex = -1;
	}

	// Selected digit: green and a little larger; the others dim lavender. Static,
	// the underline carries the focus.
	const float BaseDigitSize = CellSize * 0.6f;
	for (int32 i = 0; i < DigitTexts.Num(); i++)
	{
		UTextRenderComponent* Text = DigitTexts[i];
		if (!Text)
		{
			continue;
		}
		if (i == SelectedIndex)
		{
			Text->SetTextRenderColor(SelectionColor.ToFColor(true));
			Text->SetWorldSize(BaseDigitSize * 1.12f);
		}
		else
		{
			Text->SetTextRenderColor(IdleDigitColor.ToFColor(true));
			Text->SetWorldSize(BaseDigitSize);
		}
	}
}
