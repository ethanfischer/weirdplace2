#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MenuUIActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UTextRenderComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UWeirdplaceGameUserSettings;

UENUM(BlueprintType)
enum class EMenuPage : uint8
{
	Pause,
	Settings,
	Graphics
};

UENUM(BlueprintType)
enum class EPauseMenuItem : uint8
{
	Resume,
	Settings,
	Graphics,
	Quit,
	Count UMETA(Hidden)
};

// Identifies which row is being interacted with on the Settings page.
// Back is a non-adjustable selectable row that returns to the Pause page.
UENUM(BlueprintType)
enum class ESettingsRow : uint8
{
	GamepadSensitivity,
	MouseSensitivity,
	Back,
	Count UMETA(Hidden)
};

// Rows on the Graphics page. Each adjustable row maps to a scalability cvar.
UENUM(BlueprintType)
enum class EGraphicsRow : uint8
{
	GlobalIllumination,
	Reflection,
	Shadow,
	ViewDistance,
	ResetToDefault,
	Back,
	Count UMETA(Hidden)
};

UCLASS(Blueprintable)
class WEIRDPLACE2_API AMenuUIActor : public AActor
{
	GENERATED_BODY()

public:
	AMenuUIActor();

	// Switch active page. Resets selection to first item of the new page and
	// toggles visibility on the page roots in place (no spawn/despawn).
	void SetPage(EMenuPage NewPage);
	EMenuPage GetCurrentPage() const { return CurrentPage; }

	// Move selection on the active page. Delta: -1 = up, +1 = down. Clamps.
	void StepSelection(int32 Delta);

	// Adjust focused sensitivity row on the Settings page. No-op on Pause
	// page or when the Back row is focused.
	void StepLeftRight(int32 Delta, UWeirdplaceGameUserSettings* Settings);

	int32 GetSelectedIndex() const;
	EPauseMenuItem GetSelectedPauseItem() const { return PauseSelection; }
	ESettingsRow GetSelectedSettingsRow() const { return SettingsSelection; }
	EGraphicsRow GetSelectedGraphicsRow() const { return GraphicsSelection; }

	// Sync sensitivity row values from settings (call when entering Settings page).
	void SyncFromSettings(UWeirdplaceGameUserSettings* Settings);

	// Read current sg.* cvar values into the Graphics page (call when entering it).
	void SyncGraphicsFromCVars();

	// Re-apply the active DeviceProfile's cvars (the baked defaults) and refresh
	// the Graphics page display.
	void ResetGraphicsToDefaults();

	// Fades both pages' visible content.
	void SetOpacity(float Opacity);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu UI", meta = (AllowPrivateAccess = "true"))
	USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu UI", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BackgroundPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu UI", meta = (AllowPrivateAccess = "true"))
	USceneComponent* PausePageRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu UI", meta = (AllowPrivateAccess = "true"))
	USceneComponent* SettingsPageRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu UI", meta = (AllowPrivateAccess = "true"))
	USceneComponent* GraphicsPageRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Layout")
	float BackgroundPadding = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Materials")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.05f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Materials")
	FLinearColor FocusedValueColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Materials")
	FLinearColor UnfocusedValueColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);

private:
	struct FSettingsRowVisuals
	{
		UTextRenderComponent* LabelText = nullptr;
		UTextRenderComponent* ValueText = nullptr;
		int32 SelectedIndex = 0;
		int32 SlotCount = 0;
	};

	struct FGraphicsRowVisuals
	{
		UTextRenderComponent* RowText = nullptr;
		int32 SelectedIndex = 0;
	};

	static constexpr int32 SettingsRowCount = static_cast<int32>(ESettingsRow::Count);
	static constexpr int32 PauseItemCount = static_cast<int32>(EPauseMenuItem::Count);
	static constexpr int32 GraphicsRowCount = static_cast<int32>(EGraphicsRow::Count);
	static constexpr int32 GraphicsQualityLevels = 4; // 0=Low, 1=Medium, 2=High, 3=Epic

	FSettingsRowVisuals SettingsRows[SettingsRowCount];
	FGraphicsRowVisuals GraphicsRows[GraphicsRowCount];

	EMenuPage CurrentPage = EMenuPage::Pause;
	EPauseMenuItem PauseSelection = EPauseMenuItem::Resume;
	ESettingsRow SettingsSelection = ESettingsRow::GamepadSensitivity;
	EGraphicsRow GraphicsSelection = EGraphicsRow::GlobalIllumination;

	UPROPERTY()
	UStaticMesh* PlaneMesh;

	UPROPERTY()
	UMaterialInterface* SolidColorMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* BackgroundMaterial;

	// Settings page section headers
	UPROPERTY()
	UTextRenderComponent* ControllerHeaderText;

	UPROPERTY()
	UTextRenderComponent* MouseKBHeaderText;

	// Settings page Back item
	UPROPERTY()
	UTextRenderComponent* SettingsBackText;

	// Build stamp shown at the bottom of the menu (both pages). Sanity-check
	// that the running binary matches the source you just changed.
	UPROPERTY()
	UTextRenderComponent* BuildStampText;

	// Pause page items
	UPROPERTY()
	UTextRenderComponent* PausedHeaderText;

	UPROPERTY()
	UTextRenderComponent* PauseResumeText;

	UPROPERTY()
	UTextRenderComponent* PauseSettingsText;

	UPROPERTY()
	UTextRenderComponent* PauseGraphicsText;

	UPROPERTY()
	UTextRenderComponent* PauseQuitText;

	// Graphics page items
	UPROPERTY()
	UTextRenderComponent* GraphicsHeaderText;

	UPROPERTY()
	UTextRenderComponent* GraphicsResetText;

	UPROPERTY()
	UTextRenderComponent* GraphicsBackText;

	float CurrentOpacity = 1.0f;

	void BuildPausePage();
	void BuildSettingsPage();
	void BuildSettingsRow(ESettingsRow Row, float LabelZ, float ValueZ, const FString& Label);
	void BuildGraphicsPage();
	void BuildGraphicsRow(EGraphicsRow Row, float RowZ, const FString& Label);

	// sg.* cvar name + label for a graphics row.
	static const TCHAR* GetGraphicsCVarName(EGraphicsRow Row);
	static FString GetGraphicsQualityLabel(int32 QualityLevel);
	int32 GetGraphicsCVarValue(EGraphicsRow Row) const;
	void SetGraphicsCVarValue(EGraphicsRow Row, int32 Value);

	void UpdateBackgroundSize();
	void ApplyPageVisibility();
	void UpdateFocusColors();

	int32 GetSlotCountForRow(ESettingsRow Row) const;
	int32 ValueToSlotIndex(ESettingsRow Row, float Value) const;
	float SlotIndexToValue(ESettingsRow Row, int32 Index) const;

	float GetSettingValue(ESettingsRow Row, UWeirdplaceGameUserSettings* Settings) const;
	void SetSettingValue(ESettingsRow Row, float Value, UWeirdplaceGameUserSettings* Settings);
};
