#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryUIActor.generated.h"

class UInventoryComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

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

protected:
	virtual void BeginPlay() override;

	// Root component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	// Background panel mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BackgroundPanel;

	// Text component for item name
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTextRenderComponent* ItemNameText;

	// Text component for item counter (e.g., "3/12")
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTextRenderComponent* ItemCounterText;

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

	// Selection highlight color
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	FLinearColor SelectionColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);

private:
	UPROPERTY()
	UInventoryComponent* InventoryComponent;

	int32 SelectedIndex = 0;
	int32 GridColumns = 4;
	int32 GridRows = 3;
	float CurrentOpacity = 1.0f;

	// Empty slot meshes (always visible)
	UPROPERTY()
	TArray<UStaticMeshComponent*> SlotMeshes;

	// Spawned item thumbnail meshes (on top of slots)
	UPROPERTY()
	TArray<UStaticMeshComponent*> ThumbnailMeshes;

	// Selection highlight mesh
	UPROPERTY()
	UStaticMeshComponent* SelectionHighlight;

	// Cached plane mesh
	UPROPERTY()
	UStaticMesh* PlaneMesh;

	// Dynamic material for background
	UPROPERTY()
	UMaterialInstanceDynamic* BackgroundMaterial;

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

	// Update the background panel size
	void UpdateBackgroundSize();

	// Update item counter text
	void UpdateItemCounter();

	// Calculate position for slot at index
	FVector CalculateSlotPosition(int32 Index) const;

	// Calculate grid dimensions
	float GetGridWidth() const;
	float GetGridHeight() const;
};
