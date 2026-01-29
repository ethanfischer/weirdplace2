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

	// Navigation input handlers
	UFUNCTION(BlueprintCallable, Category = "Inventory UI|Input")
	void NavigateUp();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI|Input")
	void NavigateDown();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI|Input")
	void NavigateLeft();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI|Input")
	void NavigateRight();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI|Input")
	void ConfirmSelection();

	// Get currently selected index
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory UI")
	int32 GetSelectedIndex() const { return SelectedIndex; }

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

private:
	// Current state
	UPROPERTY()
	EInventoryUIState CurrentState = EInventoryUIState::Closed;

	// Animation progress (0 = closed, 1 = open)
	float AnimationProgress = 0.0f;

	// Currently selected grid index
	int32 SelectedIndex = 0;

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

	// Bind/unbind navigation input
	void BindNavigationInput();
	void UnbindNavigationInput();

	// Handle inventory changes (refresh UI)
	UFUNCTION()
	void OnInventoryChanged(const TArray<FName>& CurrentItems);

	// Clamp selected index to valid range
	void ClampSelectedIndex();
};
