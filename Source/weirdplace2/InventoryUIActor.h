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

	// Text component for item name
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTextRenderComponent* ItemNameText;

	// Size of each thumbnail
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	float ThumbnailSize = 8.0f;

	// Spacing between thumbnails
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	float ThumbnailSpacing = 2.0f;

	// Material for selection highlight border
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Materials")
	UMaterialInterface* SelectionBorderMaterial;

private:
	UPROPERTY()
	UInventoryComponent* InventoryComponent;

	int32 SelectedIndex = 0;
	int32 GridColumns = 4;
	float CurrentOpacity = 1.0f;

	// Spawned thumbnail meshes
	UPROPERTY()
	TArray<UStaticMeshComponent*> ThumbnailMeshes;

	// Selection highlight mesh
	UPROPERTY()
	UStaticMeshComponent* SelectionHighlight;

	// Cached plane mesh
	UPROPERTY()
	UStaticMesh* PlaneMesh;

	// Create thumbnail meshes for items
	void CreateThumbnails();

	// Clear existing thumbnails
	void ClearThumbnails();

	// Update selection highlight position
	void UpdateSelectionHighlight();

	// Calculate position for item at index
	FVector CalculateItemPosition(int32 Index) const;
};
