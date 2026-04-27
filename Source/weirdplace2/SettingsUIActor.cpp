#include "SettingsUIActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "WeirdplaceGameUserSettings.h"

// ---------------------------------------------------------------------------
// Layout constants (Z increases upward)
// ---------------------------------------------------------------------------
static constexpr float kControllerHeaderZ =  18.0f;
static constexpr float kControllerLabelZ  =  12.0f;
static constexpr float kControllerValueZ  =   6.0f;
static constexpr float kMouseKBHeaderZ    =  -4.0f;
static constexpr float kMouseKBLabelZ     = -10.0f;
static constexpr float kMouseKBValueZ     = -16.0f;

ASettingsUIActor::ASettingsUIActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshAsset.Succeeded())
	{
		PlaneMesh = PlaneMeshAsset.Object;
	}

	// Background panel.
	BackgroundPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackgroundPanel"));
	BackgroundPanel->SetupAttachment(RootSceneComponent);
	if (PlaneMesh)
	{
		BackgroundPanel->SetStaticMesh(PlaneMesh);
	}
	BackgroundPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackgroundPanel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	BackgroundPanel->SetRelativeLocation(FVector(1.0f, 0.0f, 0.0f));

	// Section headers.
	ControllerHeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ControllerHeaderText"));
	ControllerHeaderText->SetupAttachment(RootSceneComponent);
	ControllerHeaderText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	ControllerHeaderText->SetWorldSize(3.5f);
	ControllerHeaderText->SetTextRenderColor(FColor::White);
	ControllerHeaderText->SetHorizontalAlignment(EHTA_Center);
	ControllerHeaderText->SetVerticalAlignment(EVRTA_TextCenter);
	ControllerHeaderText->SetText(FText::FromString(TEXT("CONTROLLER")));

	MouseKBHeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MouseKBHeaderText"));
	MouseKBHeaderText->SetupAttachment(RootSceneComponent);
	MouseKBHeaderText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	MouseKBHeaderText->SetWorldSize(3.5f);
	MouseKBHeaderText->SetTextRenderColor(FColor::White);
	MouseKBHeaderText->SetHorizontalAlignment(EHTA_Center);
	MouseKBHeaderText->SetVerticalAlignment(EVRTA_TextCenter);
	MouseKBHeaderText->SetText(FText::FromString(TEXT("MOUSE / KEYBOARD")));
}

void ASettingsUIActor::BeginPlay()
{
	Super::BeginPlay();

	SolidColorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_SolidColor.M_SolidColor"));
	if (!SolidColorMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("ASettingsUIActor: /Game/Materials/M_SolidColor not found."));
		return;
	}

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
	UpdateFocusColors();
}

// ---------------------------------------------------------------------------
// Build visuals
// ---------------------------------------------------------------------------

void ASettingsUIActor::BuildVisuals()
{
	if (ControllerHeaderText)
	{
		ControllerHeaderText->SetRelativeLocation(FVector(0.0f, 0.0f, kControllerHeaderZ));
	}
	if (MouseKBHeaderText)
	{
		MouseKBHeaderText->SetRelativeLocation(FVector(0.0f, 0.0f, kMouseKBHeaderZ));
	}

	BuildRow(ESettingsRow::GamepadSensitivity, kControllerLabelZ, kControllerValueZ, TEXT("Look Sensitivity"));
	BuildRow(ESettingsRow::MouseSensitivity,   kMouseKBLabelZ,   kMouseKBValueZ,    TEXT("Look Sensitivity"));
}

void ASettingsUIActor::BuildRow(ESettingsRow Row, float LabelZ, float ValueZ, const FString& Label)
{
	const int32 RowIdx = static_cast<int32>(Row);
	FSettingsRowVisuals& R = Rows[RowIdx];
	R.SlotCount = GetSlotCountForRow(Row);

	// Label (centered).
	R.LabelText = NewObject<UTextRenderComponent>(this);
	R.LabelText->SetupAttachment(RootSceneComponent);
	R.LabelText->RegisterComponent();
	R.LabelText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	R.LabelText->SetWorldSize(2.8f);
	R.LabelText->SetTextRenderColor(FColor(200, 200, 200));
	R.LabelText->SetHorizontalAlignment(EHTA_Center);
	R.LabelText->SetVerticalAlignment(EVRTA_TextCenter);
	R.LabelText->SetRelativeLocation(FVector(0.0f, 0.0f, LabelZ));
	R.LabelText->SetText(FText::FromString(Label));

	// Value (centered, below label).
	R.ValueText = NewObject<UTextRenderComponent>(this);
	R.ValueText->SetupAttachment(RootSceneComponent);
	R.ValueText->RegisterComponent();
	R.ValueText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	R.ValueText->SetWorldSize(3.0f);
	R.ValueText->SetTextRenderColor(FColor(200, 200, 200));
	R.ValueText->SetHorizontalAlignment(EHTA_Center);
	R.ValueText->SetVerticalAlignment(EVRTA_TextCenter);
	R.ValueText->SetRelativeLocation(FVector(0.0f, 0.0f, ValueZ));
	R.ValueText->SetText(FText::FromString(TEXT("1.00x")));
}

// ---------------------------------------------------------------------------
// Background sizing
// ---------------------------------------------------------------------------

void ASettingsUIActor::UpdateBackgroundSize()
{
	if (!BackgroundPanel)
	{
		return;
	}

	const float TotalWidth  = 50.0f + BackgroundPadding * 2.0f;
	const float TopZ        = kControllerHeaderZ + 6.0f;
	const float BottomZ     = kMouseKBValueZ - 6.0f;
	const float TotalHeight = (TopZ - BottomZ) + BackgroundPadding * 2.0f;
	const float CenterZ     = (TopZ + BottomZ) * 0.5f;

	BackgroundPanel->SetRelativeScale3D(FVector(TotalHeight * 0.01f, TotalWidth * 0.01f, 1.0f));
	BackgroundPanel->SetRelativeLocation(FVector(1.0f, 0.0f, CenterZ));
}

// ---------------------------------------------------------------------------
// Focus colors — focused row value is yellow, unfocused is gray
// ---------------------------------------------------------------------------

void ASettingsUIActor::UpdateFocusColors()
{
	const FColor Focused   = FocusedValueColor.ToFColor(true);
	const FColor Unfocused = UnfocusedValueColor.ToFColor(true);

	for (int32 i = 0; i < RowCount; i++)
	{
		FSettingsRowVisuals& R = Rows[i];
		if (!R.ValueText)
		{
			continue;
		}
		const bool bFocused = (static_cast<ESettingsRow>(i) == FocusedRow);
		R.ValueText->SetTextRenderColor(bFocused ? Focused : Unfocused);
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ASettingsUIActor::SyncFromSettings(UWeirdplaceGameUserSettings* Settings)
{
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("ASettingsUIActor::SyncFromSettings - null Settings"));
		return;
	}
	for (int32 i = 0; i < RowCount; i++)
	{
		const ESettingsRow Row = static_cast<ESettingsRow>(i);
		const float Value = GetSettingValue(Row, Settings);
		Rows[i].SelectedIndex = ValueToSlotIndex(Row, Value);
		if (Rows[i].ValueText)
		{
			Rows[i].ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), Value)));
		}
	}
	UpdateFocusColors();
}

float ASettingsUIActor::StepSelection(int32 Delta, UWeirdplaceGameUserSettings* Settings)
{
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("ASettingsUIActor::StepSelection - null Settings"));
		return 0.0f;
	}

	const int32 RowIdx = static_cast<int32>(FocusedRow);
	FSettingsRowVisuals& R = Rows[RowIdx];

	const int32 NewIndex = FMath::Clamp(R.SelectedIndex + Delta, 0, R.SlotCount - 1);
	if (NewIndex == R.SelectedIndex)
	{
		return SlotIndexToValue(FocusedRow, R.SelectedIndex);
	}

	R.SelectedIndex = NewIndex;
	const float NewValue = SlotIndexToValue(FocusedRow, R.SelectedIndex);
	SetSettingValue(FocusedRow, NewValue, Settings);
	const float Snapped = GetSettingValue(FocusedRow, Settings);
	if (R.ValueText)
	{
		R.ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), Snapped)));
	}
	return Snapped;
}

void ASettingsUIActor::StepFocusedRow(int32 Delta)
{
	const int32 NewIdx = FMath::Clamp(static_cast<int32>(FocusedRow) + Delta, 0, RowCount - 1);
	FocusedRow = static_cast<ESettingsRow>(NewIdx);
	UpdateFocusColors();
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

	const uint8 Alpha = FMath::Clamp(static_cast<int32>(Opacity * 255), 0, 255);
	const FColor HeaderColor(255, 255, 255, Alpha);
	const FColor DimColor(200, 200, 200, Alpha);

	if (ControllerHeaderText) ControllerHeaderText->SetTextRenderColor(HeaderColor);
	if (MouseKBHeaderText)    MouseKBHeaderText->SetTextRenderColor(HeaderColor);

	for (int32 i = 0; i < RowCount; i++)
	{
		FSettingsRowVisuals& R = Rows[i];
		if (R.LabelText) R.LabelText->SetTextRenderColor(DimColor);
		if (R.ValueText)
		{
			const bool bFocused = (static_cast<ESettingsRow>(i) == FocusedRow);
			FColor ValColor = bFocused ? FocusedValueColor.ToFColor(true) : UnfocusedValueColor.ToFColor(true);
			ValColor.A = Alpha;
			R.ValueText->SetTextRenderColor(ValColor);
		}
	}
}

// ---------------------------------------------------------------------------
// Settings read/write
// ---------------------------------------------------------------------------

float ASettingsUIActor::GetSettingValue(ESettingsRow Row, UWeirdplaceGameUserSettings* Settings) const
{
	switch (Row)
	{
	case ESettingsRow::GamepadSensitivity: return Settings->GetGamepadLookSensitivity();
	case ESettingsRow::MouseSensitivity:   return Settings->GetMouseLookSensitivity();
	default: return 1.0f;
	}
}

void ASettingsUIActor::SetSettingValue(ESettingsRow Row, float Value, UWeirdplaceGameUserSettings* Settings)
{
	switch (Row)
	{
	case ESettingsRow::GamepadSensitivity: Settings->SetGamepadLookSensitivity(Value); break;
	case ESettingsRow::MouseSensitivity:   Settings->SetMouseLookSensitivity(Value);   break;
	default: break;
	}
}

// ---------------------------------------------------------------------------
// Value ↔ index math (still needed to step in discrete increments)
// ---------------------------------------------------------------------------

int32 ASettingsUIActor::GetSlotCountForRow(ESettingsRow Row) const
{
	float Min, Max, Snap;
	switch (Row)
	{
	case ESettingsRow::GamepadSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinGamepadLookSensitivity;
		Max  = UWeirdplaceGameUserSettings::MaxGamepadLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::GamepadLookSensitivitySnap;
		break;
	case ESettingsRow::MouseSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinMouseLookSensitivity;
		Max  = UWeirdplaceGameUserSettings::MaxMouseLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::MouseLookSensitivitySnap;
		break;
	default:
		return 1;
	}
	return FMath::RoundToInt((Max - Min) / Snap) + 1;
}

int32 ASettingsUIActor::ValueToSlotIndex(ESettingsRow Row, float Value) const
{
	float Min, Snap;
	switch (Row)
	{
	case ESettingsRow::GamepadSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinGamepadLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::GamepadLookSensitivitySnap;
		break;
	case ESettingsRow::MouseSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinMouseLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::MouseLookSensitivitySnap;
		break;
	default:
		return 0;
	}
	const int32 Index = FMath::RoundToInt((Value - Min) / Snap);
	return FMath::Clamp(Index, 0, GetSlotCountForRow(Row) - 1);
}

float ASettingsUIActor::SlotIndexToValue(ESettingsRow Row, int32 Index) const
{
	float Min, Snap;
	switch (Row)
	{
	case ESettingsRow::GamepadSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinGamepadLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::GamepadLookSensitivitySnap;
		break;
	case ESettingsRow::MouseSensitivity:
		Min  = UWeirdplaceGameUserSettings::MinMouseLookSensitivity;
		Snap = UWeirdplaceGameUserSettings::MouseLookSensitivitySnap;
		break;
	default:
		return 1.0f;
	}
	return Min + Index * Snap;
}
