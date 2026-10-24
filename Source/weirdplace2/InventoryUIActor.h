#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryUIActor.generated.h"

class UInventoryComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;

// World-space inventory strip: a translucent violet pane holding one thumbnail
// per carried item (a horizontally scrolling window of GridColumns items), the
// selected item's name top-left, and a neon-green underline beneath the
// selected thumbnail. Keyboard-driven: no scroll hints, no counters. The
// panel only ever shows real items — the inventory is compact, so there are
// no empty slots to draw; with nothing carried it just says "Empty".
UCLASS()
class WEIRDPLACE2_API AInventoryUIActor : public AActor
{
	GENERATED_BODY()

public:
	AInventoryUIActor();

	// Set the inventory component to display
	void SetInventoryComponent(UInventoryComponent* InInventoryComponent);

	// Set the currently selected VISIBLE slot (0 = leftmost shown item)
	void SetSelectedIndex(int32 Index);

	// Set number of columns in the visible window
	void SetGridColumns(int32 Columns);

	// Set number of rows (only 1 is laid out; kept for the component's config)
	void SetGridRows(int32 Rows);

	// Set the horizontal scroll offset (absolute index of the leftmost visible
	// item). The strip shows items [ScrollOffset, ScrollOffset + GetTotalSlots()).
	// Rebuilds the thumbnail window when the offset changes.
	void SetScrollOffset(int32 Offset);

	// Capacity of the visible window: GridColumns x GridRows
	int32 GetTotalSlots() const { return GridColumns * GridRows; }

	// Rebuild everything from the inventory contents
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void RefreshDisplay();

	// Set opacity (for animation)
	void SetOpacity(float Opacity);

	// Show/hide the whole strip. Hiding propagates to every child; showing
	// re-applies the frame/hint visibility, which depends on inventory state.
	void SetShown(bool bShown);

	// Show ItemID's display name above the strip ("Empty" for NAME_None)
	void SetItemName(const FName& ItemID);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	// Dark backing panel, sized to the items currently shown
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BackgroundPanel;

	// Selected item's name, centered above the thumbnails
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTextRenderComponent* ItemNameText;

	// Size of each thumbnail (world units)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	float ThumbnailSize = 8.0f;

	// Gap between thumbnails
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	float ThumbnailSpacing = 2.0f;

	// Panel padding around the thumbnails (sides and bottom)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	float BackgroundPadding = 4.0f;

	// Height of the name band above the thumbnails
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	float TitleBandHeight = 5.0f;

	// Name text world size
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	float TitleTextSize = 1.5f;

	// Background panel color (alpha is multiplied by the open/close fade)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.02f, 0.11f, 0.55f);

	// Underline under the selected thumbnail (unlit emissive; the forecourt neon green)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	FLinearColor SelectionColor = FLinearColor(0.35f, 1.0f, 0.25f, 1.0f);

	// Title text color (lavender, like the night sky)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	FLinearColor TitleColor = FLinearColor(0.9f, 0.86f, 1.0f, 1.0f);

	// How much larger the selected thumbnail grows
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Hover")
	float HoverScaleMultiplier = 1.15f;

	// Speed of the grow animation on selection change
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Hover")
	float HoverAnimationSpeed = 8.0f;

	// Underline thickness and its gap below the grown thumbnail (world units)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Hover")
	float UnderlineThickness = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Hover")
	float UnderlineGap = 1.0f;

private:
	UPROPERTY()
	UInventoryComponent* InventoryComponent;

	int32 SelectedIndex = 0;
	int32 PreviousSelectedIndex = -1;
	int32 GridColumns = 3;
	int32 GridRows = 1;
	float CurrentOpacity = 1.0f;

	// Absolute index of the leftmost visible item; visible slot p shows item (ScrollOffset + p).
	int32 ScrollOffset = 0;

	// Items currently drawn (<= GetTotalSlots()). The pane is sized for the whole
	// window regardless, so it never resizes as items come and go.
	int32 VisibleCount = 0;

	float HoverAnimationProgress = 0.0f;

	// One per visible slot; null where the window runs past the last item.
	UPROPERTY()
	TArray<UStaticMeshComponent*> ThumbnailMeshes;

	// Green underline beneath the selected thumbnail
	UPROPERTY()
	UStaticMeshComponent* SelectionHighlight;

	UPROPERTY()
	UStaticMesh* PlaneMesh;

	UPROPERTY()
	UMaterialInstanceDynamic* BackgroundMaterial;

	// Unlit solid-color material for the selection frame (M_SolidColor)
	UPROPERTY()
	UMaterialInterface* SolidColorMaterial;

	void CreateThumbnails();
	void ClearThumbnails();
	void UpdateSelectionHighlight();
	void UpdateHoverAnimation(float DeltaTime);
	void UpdateBackgroundSize();
	void SetThumbnailScale(UStaticMeshComponent* Mesh, float Scale) const;

	// Center of visible slot Index, with the shown items centered on the panel
	FVector CalculateSlotPosition(int32 Index) const;

	// Width of the full window of slots
	float GetGridWidth() const;
};
