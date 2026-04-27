#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SettingsUIActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UTextRenderComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UWeirdplaceGameUserSettings;

// Identifies which setting row is being interacted with.
UENUM(BlueprintType)
enum class ESettingsRow : uint8
{
	GamepadSensitivity,
	MouseSensitivity,
	Count UMETA(Hidden)
};

UCLASS(Blueprintable)
class WEIRDPLACE2_API ASettingsUIActor : public AActor
{
	GENERATED_BODY()

public:
	ASettingsUIActor();

	// Step the value within the focused row by Delta (+1 or -1).
	// Persists into Settings and refreshes visuals. Returns the new snapped value.
	float StepSelection(int32 Delta, UWeirdplaceGameUserSettings* Settings);

	// Move focus to a different row. Delta: -1 = up, +1 = down. Clamps.
	void StepFocusedRow(int32 Delta);

	// Sync all rows from the current settings values (call on open).
	void SyncFromSettings(UWeirdplaceGameUserSettings* Settings);

	ESettingsRow GetFocusedRow() const { return FocusedRow; }

	void SetOpacity(float Opacity);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings UI", meta = (AllowPrivateAccess = "true"))
	USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings UI", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BackgroundPanel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Layout")
	float BackgroundPadding = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Materials")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.05f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Materials")
	FLinearColor FocusedValueColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Materials")
	FLinearColor UnfocusedValueColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);

private:
	// Per-row state.
	struct FSettingsRowVisuals
	{
		UTextRenderComponent* LabelText = nullptr;
		UTextRenderComponent* ValueText = nullptr;
		int32 SelectedIndex = 0;
		int32 SlotCount = 0;
	};

	static constexpr int32 RowCount = static_cast<int32>(ESettingsRow::Count);

	FSettingsRowVisuals Rows[RowCount];
	ESettingsRow FocusedRow = ESettingsRow::GamepadSensitivity;

	UPROPERTY()
	UStaticMesh* PlaneMesh;

	UPROPERTY()
	UMaterialInterface* SolidColorMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* BackgroundMaterial;

	UPROPERTY()
	UTextRenderComponent* ControllerHeaderText;

	UPROPERTY()
	UTextRenderComponent* MouseKBHeaderText;

	float CurrentOpacity = 1.0f;

	// Row config helpers.
	int32 GetSlotCountForRow(ESettingsRow Row) const;
	int32 ValueToSlotIndex(ESettingsRow Row, float Value) const;
	float SlotIndexToValue(ESettingsRow Row, int32 Index) const;

	void BuildVisuals();
	void BuildRow(ESettingsRow Row, float LabelZ, float ValueZ, const FString& Label);
	void UpdateBackgroundSize();
	void UpdateFocusColors();

	float GetSettingValue(ESettingsRow Row, UWeirdplaceGameUserSettings* Settings) const;
	void SetSettingValue(ESettingsRow Row, float Value, UWeirdplaceGameUserSettings* Settings);
};
