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

UCLASS(Blueprintable)
class WEIRDPLACE2_API ASettingsUIActor : public AActor
{
	GENERATED_BODY()

public:
	ASettingsUIActor();

	// Sync visuals (selection highlight + value label) to a sensitivity value (already snapped).
	UFUNCTION(BlueprintCallable, Category = "Settings UI")
	void RefreshFromSettings(float SnappedSensitivityValue);

	// Step the selection by Delta (typically +1 or -1), clamp, persist into Settings,
	// and refresh visuals. Returns the snapped value now applied.
	UFUNCTION(BlueprintCallable, Category = "Settings UI")
	float StepSelection(int32 Delta, UWeirdplaceGameUserSettings* Settings);

	// Reset SelectedIndex from the current settings value (call on open).
	void SyncFromSettings(UWeirdplaceGameUserSettings* Settings);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Settings UI")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	// Mirror of AInventoryUIActor::SetOpacity for animation fade-in.
	void SetOpacity(float Opacity);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings UI", meta = (AllowPrivateAccess = "true"))
	USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings UI", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* BackgroundPanel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings UI", meta = (AllowPrivateAccess = "true"))
	UTextRenderComponent* TitleText;

	// Layout, mirroring the inventory's pattern.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Layout")
	float SlotSize = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Layout")
	float SlotSpacing = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Layout")
	float BackgroundPadding = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Materials")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.05f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Materials")
	FLinearColor EmptySlotColor = FLinearColor(0.1f, 0.1f, 0.12f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Materials")
	FLinearColor SelectionColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);

private:
	UPROPERTY()
	UStaticMesh* PlaneMesh;

	UPROPERTY()
	UMaterialInterface* SolidColorMaterial;

	UPROPERTY()
	TArray<UStaticMeshComponent*> SlotMeshes;

	UPROPERTY()
	UStaticMeshComponent* SelectionHighlight;

	UPROPERTY()
	UMaterialInstanceDynamic* BackgroundMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* SelectionMaterial;

	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> SlotMaterials;

	int32 SelectedIndex = 0;
	float CurrentOpacity = 1.0f;

	int32 GetSlotCount() const;
	int32 ValueToSlotIndex(float Value) const;
	float SlotIndexToValue(int32 Index) const;
	FVector CalculateSlotPosition(int32 Index) const;
	float GetGridWidth() const;
	float GetGridHeight() const;

	void BuildVisuals();
	void UpdateBackgroundSize();
	void UpdateSelectionHighlight();
};
