#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryUIComponent.generated.h"

class UInventoryComponent;
class AInventoryUIActor;

// State machine for inventory UI animation
UENUM(BlueprintType)
enum class EInventoryUIState : uint8
{
	Closed,
	Opening,
	Open,
	Closing
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UInventoryUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryUIComponent();

	// Toggle inventory UI open/closed
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ToggleInventoryUI();

	// Open inventory UI
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void OpenInventoryUI();

	// Close inventory UI
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void CloseInventoryUI();

	// Check if inventory UI is currently open (or opening)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory UI")
	bool IsInventoryOpen() const;

	// Confirm selection (E key / click)
	UFUNCTION(BlueprintCallable, Category = "Inventory UI|Input")
	void ConfirmSelection();

	// Get currently selected index
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory UI")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	// True when the center reticle ray is currently over the inventory grid bounds
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory UI")
	bool IsReticleOverGrid() const { return bReticleOverGrid; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Configuration ---

	// Class to spawn for the inventory UI actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Setup")
	TSubclassOf<AInventoryUIActor> InventoryUIActorClass;

	// Distance in front of camera to position inventory
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Position")
	float InventoryDistance = 50.0f;

	// Vertical offset (negative = below center)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Position")
	float VerticalOffset = -15.0f;

	// Animation duration in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Animation")
	float AnimationDuration = 0.3f;

	// How far below to start/end the animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Animation")
	float AnimationDropDistance = 30.0f;

	// Number of columns in the grid
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	int32 GridColumns = 4;

	// Number of rows in the grid
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Layout")
	int32 GridRows = 3;

	// Sound to play when opening the inventory
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Audio")
	USoundBase* MenuOpenSound;

	// Sound to play when closing the inventory
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Audio")
	USoundBase* MenuCloseSound;

	// Sound to play when selecting an item
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI|Audio")
	USoundBase* MenuItemSelectedSound;

private:
	// Current state
	UPROPERTY()
	EInventoryUIState CurrentState = EInventoryUIState::Closed;

	// Animation progress (0 = closed, 1 = open)
	float AnimationProgress = 0.0f;

	// Currently selected grid index
	int32 SelectedIndex = 0;

	// Whether reticle is currently over any inventory slot area
	bool bReticleOverGrid = false;

	// Stored UI position when opened (UI stays fixed, doesn't follow camera)
	FVector StoredUIPosition;
	FRotator StoredUIRotation;

	// Spawned inventory UI actor
	UPROPERTY()
	AInventoryUIActor* InventoryUIActor;

	// Cached reference to owner's inventory component
	UPROPERTY()
	UInventoryComponent* InventoryComponent;

	// Spawn the inventory UI actor
	void SpawnInventoryUIActor();

	// Destroy the inventory UI actor
	void DestroyInventoryUIActor();

	// Update inventory actor position based on camera and animation
	void UpdateInventoryPosition();

	// Bind/unbind confirm input
	void BindConfirmInput();
	void UnbindConfirmInput();

	// Freeze/unfreeze player movement
	void FreezePlayerMovement();
	void UnfreezePlayerMovement();

	// Update selection based on where player is looking (reticle)
	void UpdateReticleSelection();

	// Calculate which grid slot the reticle is pointing at
	int32 CalculateSlotFromReticle() const;

	// Handle inventory changes (refresh UI)
	UFUNCTION()
	void OnInventoryChanged(const TArray<FName>& CurrentItems);

	// Clamp selected index to valid range
	void ClampSelectedIndex();

	// Dirty flag - true when inventory changed and UI needs refresh
	bool bInventoryNeedsRefresh = true;
};
