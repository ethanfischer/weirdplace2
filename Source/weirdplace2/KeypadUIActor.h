#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeypadUIActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;

// World-space numpad UI, modeled on AInventoryUIActor. Renders a fixed 3x3 grid
// of digit cells (1-9) with a pulsing selection highlight, plus a row of
// CodeLength "fill slots" below the grid that fill in as the player enters
// digits. Self-illuminated (unlit text + emissive slots) like the inventory UI.
UCLASS()
class WEIRDPLACE2_API AKeypadUIActor : public AActor
{
	GENERATED_BODY()

public:
	AKeypadUIActor();

	// Build the digit grid + fill-slot row. Call once after spawn (the component does).
	void BuildKeypad();

	// Set the highlighted cell (0..8).
	void SetSelectedCell(int32 CellIndex);

	// Set how many fill slots to show (the code length). Rebuilds the fill row.
	void SetCodeLength(int32 Len);

	// Update the fill-slot digits from the code entered so far.
	void SetEnteredCode(const FString& Code);

	// Opacity (0..1) for the open/close animation.
	void SetOpacity(float Opacity);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BackgroundPanel;

	// --- Layout ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Layout")
	float CellSize = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Layout")
	float CellSpacing = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Layout")
	float BackgroundPadding = 4.0f;

	// --- Materials / Colors ---
	// M_SolidColor is unlit + emissive and the hallway is dark, so eye adaptation
	// lifts these a lot: 0.1 reads as white in-game. Keep them near black.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Materials")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.02f, 0.11f, 0.55f); // translucent violet pane (M_ShadowGlass)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Materials")
	FLinearColor CellColor = FLinearColor(0.012f, 0.012f, 0.015f, 0.6f);

	// Empty fill-slot color (no digit entered yet).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Materials")
	FLinearColor EmptySlotColor = FLinearColor(0.012f, 0.012f, 0.015f, 0.6f);

	// Filled fill-slot color (digit entered).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Materials")
	FLinearColor FilledSlotColor = FLinearColor(0.04f, 0.14f, 0.03f, 0.9f); // dim neon green

	// Material for cells + fill slots (recommended: a solid-color MI). Falls back to
	// /Game/Materials/M_SolidColor like the inventory UI.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Materials")
	UMaterialInterface* SlotMaterial = nullptr;

	// Material for the selection highlight (recommended: a solid-color MI). Falls
	// back to M_SolidColor tinted with SelectionColor if unset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Materials")
	UMaterialInterface* SelectionHighlightMaterial = nullptr;

	// Glow color of the active digit (pulses).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Materials")
	FLinearColor SelectionColor = FLinearColor(0.35f, 1.0f, 0.25f, 1.0f); // forecourt neon green

	// Color of the non-selected digits (kept dim so the active one stands out).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Materials")
	FLinearColor IdleDigitColor = FLinearColor(0.45f, 0.42f, 0.55f, 1.0f); // dim lavender

	// --- Hover / selection feedback ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Hover")
	float HoverScaleMultiplier = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad|Hover")
	float HoverAnimationSpeed = 8.0f;


private:
	static constexpr int32 GridColumns = 3;
	static constexpr int32 GridRows = 3;
	static constexpr int32 NumDigits = 9;

	int32 SelectedIndex = 0;
	int32 PreviousSelectedIndex = -1;
	int32 CodeLength = 4;
	float CurrentOpacity = 1.0f;
	float HoverAnimationProgress = 0.0f;

	UPROPERTY()
	TArray<UStaticMeshComponent*> CellMeshes;

	// Per-cell dynamic materials, so the selected cell can glow (slot-aligned with CellMeshes).
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> CellMaterials;

	UPROPERTY()
	TArray<UTextRenderComponent*> DigitTexts;

	UPROPERTY()
	TArray<UStaticMeshComponent*> FillSlotMeshes;

	UPROPERTY()
	TArray<UTextRenderComponent*> FillSlotTexts;

	UPROPERTY()
	UStaticMeshComponent* SelectionHighlight;

	// Dynamic material for the highlight (so its emissive can pulse).
	UPROPERTY()
	UMaterialInstanceDynamic* SelectionMaterial;

	UPROPERTY()
	UStaticMesh* PlaneMesh;

	UPROPERTY()
	UMaterialInstanceDynamic* BackgroundMaterial;

	void CreateCells();
	void CreateFillSlots();
	void UpdateSelectionHighlight();

	// Tint the selected cell with SelectionColor and the rest with CellColor.
	void RefreshCellColors();
	void UpdateHoverAnimation(float DeltaTime);
	void UpdateBackgroundSize();
	void ApplyUnlitTextMaterial();

	// Resolve the cell/slot material (assigned or fallback), shared by cells + fill slots.
	UMaterialInterface* ResolveSlotMaterial() const;

	FVector CalculateCellPosition(int32 Index) const;
	FVector CalculateFillSlotPosition(int32 Index) const;
	float GetGridWidth() const;
	float GetGridHeight() const;
};
