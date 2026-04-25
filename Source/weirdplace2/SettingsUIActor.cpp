#include "SettingsUIActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "WeirdplaceGameUserSettings.h"

ASettingsUIActor::ASettingsUIActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	// Reuse the same plane mesh the inventory uses, for visual parity.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshAsset.Succeeded())
	{
		PlaneMesh = PlaneMeshAsset.Object;
	}

	// Background panel (dark translucent), exactly mirroring AInventoryUIActor.
	BackgroundPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackgroundPanel"));
	BackgroundPanel->SetupAttachment(RootSceneComponent);
	if (PlaneMesh)
	{
		BackgroundPanel->SetStaticMesh(PlaneMesh);
	}
	BackgroundPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackgroundPanel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	BackgroundPanel->SetRelativeLocation(FVector(1.0f, 0.0f, 0.0f));

	// Top label — same setup the inventory uses for ItemNameTextTop.
	TitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleText"));
	TitleText->SetupAttachment(RootSceneComponent);
	TitleText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	TitleText->SetWorldSize(3.0f);
	TitleText->SetTextRenderColor(FColor::White);
	TitleText->SetHorizontalAlignment(EHTA_Center);
	TitleText->SetVerticalAlignment(EVRTA_TextCenter);
	TitleText->SetText(FText::FromString(TEXT("0.50x")));
}

void ASettingsUIActor::BeginPlay()
{
	Super::BeginPlay();

	// Attach the same M_SolidColor material the inventory falls back to.
	SolidColorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_SolidColor.M_SolidColor"));
	if (!SolidColorMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("ASettingsUIActor: /Game/Materials/M_SolidColor not found. Run create_solid_color_material.py."));
		return;
	}

	// Background gets a dynamic material so opacity can fade with the open animation,
	// matching the inventory's BackgroundMaterial flow.
	if (BackgroundPanel)
	{
		BackgroundMaterial = UMaterialInstanceDynamic::Create(SolidColorMaterial, this);
		if (BackgroundMaterial)
		{
			BackgroundMaterial->SetVectorParameterValue(FName("Color"), BackgroundColor);
			BackgroundMaterial->SetVectorParameterValue(FName("BaseColor"), BackgroundColor);
			BackgroundMaterial->SetVectorParameterValue(FName("EmissiveColor"), BackgroundColor);
			BackgroundPanel->SetMaterial(0, BackgroundMaterial);
		}
	}

	BuildVisuals();
	UpdateBackgroundSize();
	UpdateSelectionHighlight();
}

void ASettingsUIActor::BuildVisuals()
{
	if (!PlaneMesh || !SolidColorMaterial)
	{
		return;
	}

	const int32 SlotCount = GetSlotCount();

	// Empty-slot tiles, mirroring AInventoryUIActor::CreateSlots.
	for (int32 i = 0; i < SlotCount; i++)
	{
		UStaticMeshComponent* SlotMesh = NewObject<UStaticMeshComponent>(this);
		SlotMesh->SetStaticMesh(PlaneMesh);
		SlotMesh->SetupAttachment(RootSceneComponent);
		SlotMesh->RegisterComponent();
		SlotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		FVector Position = CalculateSlotPosition(i);
		Position.X += 0.3f;
		SlotMesh->SetRelativeLocation(Position);
		SlotMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

		const float Width = SlotSize * 1.05f;
		const float Height = SlotSize * 1.4f * 1.05f;
		SlotMesh->SetRelativeScale3D(FVector(Height * 0.01f, Width * 0.01f, 1.0f));

		UMaterialInstanceDynamic* SlotMat = UMaterialInstanceDynamic::Create(SolidColorMaterial, this);
		if (SlotMat)
		{
			SlotMat->SetVectorParameterValue(FName("Color"), EmptySlotColor);
			SlotMat->SetVectorParameterValue(FName("BaseColor"), EmptySlotColor);
			SlotMat->SetVectorParameterValue(FName("EmissiveColor"), EmptySlotColor);
			SlotMesh->SetMaterial(0, SlotMat);
			SlotMaterials.Add(SlotMat);
		}

		SlotMeshes.Add(SlotMesh);
	}

	// Selection highlight, mirroring AInventoryUIActor::CreateSlots highlight branch.
	SelectionHighlight = NewObject<UStaticMeshComponent>(this);
	SelectionHighlight->SetStaticMesh(PlaneMesh);
	SelectionHighlight->SetupAttachment(RootSceneComponent);
	SelectionHighlight->RegisterComponent();
	SelectionHighlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	const float HighlightWidth = SlotSize * 1.15f;
	const float HighlightHeight = SlotSize * 1.4f * 1.15f;
	SelectionHighlight->SetRelativeScale3D(FVector(HighlightHeight * 0.01f, HighlightWidth * 0.01f, 1.0f));
	SelectionHighlight->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

	SelectionMaterial = UMaterialInstanceDynamic::Create(SolidColorMaterial, this);
	if (SelectionMaterial)
	{
		SelectionMaterial->SetVectorParameterValue(FName("Color"), SelectionColor);
		SelectionMaterial->SetVectorParameterValue(FName("BaseColor"), SelectionColor);
		SelectionMaterial->SetVectorParameterValue(FName("EmissiveColor"), SelectionColor);
		SelectionHighlight->SetMaterial(0, SelectionMaterial);
	}
}

void ASettingsUIActor::UpdateBackgroundSize()
{
	if (!BackgroundPanel)
	{
		return;
	}

	const float GridW = GetGridWidth();
	const float GridH = GetGridHeight();
	const float TotalWidth = GridW + BackgroundPadding * 2.0f;
	const float TotalHeight = GridH + BackgroundPadding * 2.0f + 16.0f; // headroom for top text

	BackgroundPanel->SetRelativeScale3D(FVector(TotalHeight * 0.01f, TotalWidth * 0.01f, 1.0f));

	if (TitleText)
	{
		const float TopTextZ = GridH * 0.5f + BackgroundPadding + 2.0f;
		TitleText->SetRelativeLocation(FVector(0.0f, 0.0f, TopTextZ));
	}
}

void ASettingsUIActor::UpdateSelectionHighlight()
{
	if (!SelectionHighlight)
	{
		return;
	}

	FVector Position = CalculateSlotPosition(SelectedIndex);
	Position.X += 0.5f; // Behind the slot, like the inventory's highlight
	SelectionHighlight->SetRelativeLocation(Position);
	SelectionHighlight->SetVisibility(true);
}

void ASettingsUIActor::RefreshFromSettings(float SnappedSensitivityValue)
{
	SelectedIndex = ValueToSlotIndex(SnappedSensitivityValue);
	UpdateSelectionHighlight();

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), SnappedSensitivityValue)));
	}
}

void ASettingsUIActor::SyncFromSettings(UWeirdplaceGameUserSettings* Settings)
{
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("ASettingsUIActor::SyncFromSettings - null Settings"));
		return;
	}
	RefreshFromSettings(Settings->GetGamepadLookSensitivity());
}

float ASettingsUIActor::StepSelection(int32 Delta, UWeirdplaceGameUserSettings* Settings)
{
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("ASettingsUIActor::StepSelection - null Settings"));
		return 0.0f;
	}

	const int32 NewIndex = FMath::Clamp(SelectedIndex + Delta, 0, GetSlotCount() - 1);
	if (NewIndex == SelectedIndex)
	{
		return SlotIndexToValue(SelectedIndex);
	}

	SelectedIndex = NewIndex;
	const float NewValue = SlotIndexToValue(SelectedIndex);
	Settings->SetGamepadLookSensitivity(NewValue);
	const float Snapped = Settings->GetGamepadLookSensitivity();
	// Update visuals from the persisted (snapped) value so display matches truth.
	UpdateSelectionHighlight();
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), Snapped)));
	}
	return Snapped;
}

void ASettingsUIActor::SetOpacity(float Opacity)
{
	CurrentOpacity = Opacity;

	if (BackgroundMaterial)
	{
		FLinearColor Adjusted = BackgroundColor;
		Adjusted.A *= Opacity;
		BackgroundMaterial->SetVectorParameterValue(FName("BaseColor"), Adjusted);
	}

	if (SelectionMaterial)
	{
		SelectionMaterial->SetScalarParameterValue(FName("Opacity"), Opacity);
	}

	if (TitleText)
	{
		FColor TextColor = FColor::White;
		TextColor.A = FMath::Clamp(static_cast<int32>(Opacity * 255), 0, 255);
		TitleText->SetTextRenderColor(TextColor);
	}
}

int32 ASettingsUIActor::GetSlotCount() const
{
	const float Min = UWeirdplaceGameUserSettings::MinGamepadLookSensitivity;
	const float Max = UWeirdplaceGameUserSettings::MaxGamepadLookSensitivity;
	const float Snap = UWeirdplaceGameUserSettings::GamepadLookSensitivitySnap;
	return FMath::RoundToInt((Max - Min) / Snap) + 1;
}

int32 ASettingsUIActor::ValueToSlotIndex(float Value) const
{
	const float Min = UWeirdplaceGameUserSettings::MinGamepadLookSensitivity;
	const float Snap = UWeirdplaceGameUserSettings::GamepadLookSensitivitySnap;
	const int32 Index = FMath::RoundToInt((Value - Min) / Snap);
	return FMath::Clamp(Index, 0, GetSlotCount() - 1);
}

float ASettingsUIActor::SlotIndexToValue(int32 Index) const
{
	const float Min = UWeirdplaceGameUserSettings::MinGamepadLookSensitivity;
	const float Snap = UWeirdplaceGameUserSettings::GamepadLookSensitivitySnap;
	return Min + Index * Snap;
}

FVector ASettingsUIActor::CalculateSlotPosition(int32 Index) const
{
	// Single horizontal row.
	const int32 SlotCount = GetSlotCount();
	const float SlotPitch = SlotSize + SlotSpacing;
	const float TotalWidth = (SlotCount - 1) * SlotPitch;
	const float Y = -TotalWidth * 0.5f + Index * SlotPitch;
	return FVector(0.0f, Y, 0.0f);
}

float ASettingsUIActor::GetGridWidth() const
{
	const int32 SlotCount = GetSlotCount();
	return SlotCount * SlotSize + (SlotCount - 1) * SlotSpacing;
}

float ASettingsUIActor::GetGridHeight() const
{
	return SlotSize * 1.4f;
}
