#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryUIActor.generated.h"

class UInventoryComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;

UCLASS()
class WEIRDPLACE2_API AInventoryUIActor : public AActor
{
	GENERATED_BODY()

public:
	AInventoryUIActor();

	// Set the inventory component to display
	void SetInventoryComponent(UInventoryComponent* InInventoryComponent);

	// Set the currently selected index
	void SetSelectedIndex(int32 Index);

	// Set number of grid columns
	void SetGridColumns(int32 Columns);

	// Set number of grid rows
	void SetGridRows(int32 Rows);

	// Get total slot count
	int32 GetTotalSlots() const { return GridColumns * GridRows; }

	// Refresh the display
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void RefreshDisplay();

	// Set opacity (for animation)
	void SetOpacity(float Opacity);

	// Set the active (confirmed) item to display at bottom and show border
	void SetActiveItem(const FName& ItemID, int32 ItemIndex);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Root component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	// Background panel mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BackgroundPanel;

	// Text component for item name (top label)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTextRenderComponent* ItemNameTextTop;

	// Size of each thumbnail
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	float ThumbnailSize = 8.0f;

	// Spacing between thumbnails
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	float ThumbnailSpacing = 2.0f;

	// Padding around the grid inside the background
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	float BackgroundPadding = 4.0f;

	// Background panel color
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.05f, 0.85f);

	// Empty slot color
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	FLinearColor EmptySlotColor = FLinearColor(0.1f, 0.1f, 0.12f, 0.6f);

	// Empty slot border color
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	FLinearColor EmptySlotBorderColor = FLinearColor(0.3f, 0.3f, 0.35f, 0.8f);

	// Material used by empty slots (separate from background material)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	UMaterialInterface* SlotMaterial = nullptr;

	// Selection highlight color (hover)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	FLinearColor SelectionColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);

	// Material used by selection highlight (recommended: a solid-color MI)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	UMaterialInterface* SelectionHighlightMaterial = nullptr;

	// Active item border color (confirmed selection)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	FLinearColor ActiveItemColor = FLinearColor(0.0f, 1.0f, 0.5f, 1.0f);

	// Material used by active item border (recommended: a solid-color MI)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	UMaterialInterface* ActiveItemBorderMaterial = nullptr;

	// Hover scale multiplier (how much larger the hovered slot becomes)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Hover")
	float HoverScaleMultiplier = 1.15f;

	// Hover animation speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Hover")
	float HoverAnimationSpeed = 8.0f;

	// Selection highlight pulse speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Hover")
	float SelectionPulseSpeed = 3.0f;

	// Selection highlight pulse intensity (0-1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Hover")
	float SelectionPulseIntensity = 0.3f;

private:
	UPROPERTY()
	UInventoryComponent* InventoryComponent;

	int32 SelectedIndex = 0;
	int32 PreviousSelectedIndex = -1;
	int32 GridColumns = 3;
	int32 GridRows = 1;
	float CurrentOpacity = 1.0f;

	// Hover animation progress (0 = not hovered, 1 = fully hovered)
	float HoverAnimationProgress = 0.0f;

	// Time accumulator for pulse effect
	float PulseTime = 0.0f;

	// Empty slot meshes (always visible)
	UPROPERTY()
	TArray<UStaticMeshComponent*> SlotMeshes;

	// Spawned item thumbnail meshes (on top of slots)
	UPROPERTY()
	TArray<UStaticMeshComponent*> ThumbnailMeshes;

	// Selection highlight mesh (hover)
	UPROPERTY()
	UStaticMeshComponent* SelectionHighlight;

	// Active item border mesh (confirmed selection)
	UPROPERTY()
	UStaticMeshComponent* ActiveItemBorder;

	// Index of currently active item (-1 if none)
	int32 ActiveItemIndex = -1;

	// Cached plane mesh
	UPROPERTY()
	UStaticMesh* PlaneMesh;

	// Dynamic material for background
	UPROPERTY()
	UMaterialInstanceDynamic* BackgroundMaterial;

	// Dynamic material for selection highlight (for pulsing)
	UPROPERTY()
	UMaterialInstanceDynamic* SelectionMaterial;

	// Dynamic material for active item border
	UPROPERTY()
	UMaterialInstanceDynamic* ActiveItemMaterial;

	// Create the grid slots (empty slot visuals)
	void CreateSlots();

	// Clear existing slots
	void ClearSlots();

	// Create thumbnail meshes for items
	void CreateThumbnails();

	// Clear existing thumbnails
	void ClearThumbnails();

	// Update selection highlight position
	void UpdateSelectionHighlight();

	// Update hover animation (scale, pulse)
	void UpdateHoverAnimation(float DeltaTime);

	// Update the background panel size
	void UpdateBackgroundSize();

	// Update item counter text

	// Calculate position for slot at index
	FVector CalculateSlotPosition(int32 Index) const;

	// Calculate grid dimensions
	float GetGridWidth() const;
	float GetGridHeight() const;
};
