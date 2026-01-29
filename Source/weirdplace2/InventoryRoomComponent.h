#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/TextRenderComponent.h"
#include "InventoryItemMapping.h"
#include "MovieBoxDisplayActor.h"
#include "InventoryRoomComponent.generated.h"

class UInventoryComponent;
class UMaterialInstanceDynamic;

/**
 * DEPRECATED: This component is being replaced by UInventoryUIComponent.
 * Kept for backwards compatibility during transition.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UInventoryRoomComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryRoomComponent();

	// Toggle inventory room (called from input binding)
	UFUNCTION(BlueprintCallable, Category = "Inventory Room")
	void ToggleInventoryRoom();

	// Check if currently in inventory room
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Room")
	bool IsInInventoryRoom() const { return bIsInInventoryRoom; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Configuration Properties ---

	// Target point actor to teleport to (set in editor). If not set, uses InventoryRoomLocation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Location")
	AActor* InventoryRoomTarget;

	// Fallback location if no target point is set
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Location")
	FVector InventoryRoomLocation = FVector(50000.0f, 0.0f, 0.0f);

	// Rotation when entering inventory room (used if no target point, or as override)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Location")
	FRotator InventoryRoomRotation = FRotator::ZeroRotator;

	// Folder path for spawned inventory items in Outliner
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	FName InventoryFolderPath = FName("Inventory");

	// Mapping of inventory items to display actors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Items")
	TArray<FInventoryItemDisplayInfo> ItemDisplayMappings;

	// Display actor class for showing collected movie boxes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Items")
	TSubclassOf<AActor> MovieBoxDisplayActorClass = AMovieBoxDisplayActor::StaticClass();

	// Grid layout settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	int32 GridColumns = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	float GridSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	float GridVerticalSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	float WallOffset;

	// Height offset for displayed items (relative to room location)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	float ItemDisplayHeight = 50.0f;

	// Distance in front of player spawn point to display items
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Layout")
	float ItemDisplayDistance = 200.0f;

	// --- Item Name Text Settings ---

	// World size of the item name text
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Text")
	float TextWorldSize = 12.0f;

	// Vertical offset below the grid for the text
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Text")
	float TextVerticalOffset = 60.0f;

	// Color of the item name text
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Text")
	FColor TextColor = FColor::White;

	// Material for the text (use an unlit material to avoid shadows)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Text")
	UMaterialInterface* TextMaterial;

	// --- Background Wall Settings ---

	// The wall mesh to apply the captured background to (set in editor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Background")
	AActor* BackgroundWallActor;

	// Base material for the blurred background (must have a TextureParameter named "BackgroundTexture")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Room|Background")
	UMaterialInterface* BackgroundBlurMaterial;

private:
	// State tracking
	bool bIsInInventoryRoom = false;

	// Stored transform before entering inventory room
	FVector StoredLocation;
	FRotator StoredRotation;

	// Reference to owner's inventory component
	UPROPERTY()
	UInventoryComponent* InventoryComponent;

	// Currently spawned display actors
	UPROPERTY()
	TArray<AActor*> SpawnedDisplayActors;

	// Map from spawned actor to its cover name (for look-at detection)
	UPROPERTY()
	TMap<AActor*, FString> ActorCoverNames;

	// Text component for displaying looked-at item name
	UPROPERTY()
	UTextRenderComponent* ItemNameText;

	// Currently displayed item name (to detect changes)
	FString CurrentLookedAtItem;

	// Dynamic material instance applied to the wall
	UPROPERTY()
	UMaterialInstanceDynamic* BackgroundMaterialInstance;

	// Original material on the wall (to restore when leaving)
	UPROPERTY()
	UMaterialInterface* OriginalWallMaterial;

	// Original scale of the wall (to restore when leaving)
	FVector OriginalWallScale;

	// --- Internal Methods ---
	void TeleportToInventoryRoom();
	void TeleportBack();
	void SpawnInventoryDisplayActors();
	void DestroyInventoryDisplayActors();

	// Find display info for given inventory item ID
	const FInventoryItemDisplayInfo* GetDisplayInfo(const FName& ItemID) const;

	// Calculate spawn position for item at given index
	FVector CalculateItemPosition(int32 Index) const;

	// Handle inventory changes while in room (refresh display)
	UFUNCTION()
	void OnInventoryChanged(const TArray<FName>& CurrentItems);

	// Update the looked-at item text based on raycast
	void UpdateLookedAtItem();

	// Capture the player's current view and apply to background wall
	void CaptureAndApplyBackground();

	// Restore original wall material
	void RestoreWallMaterial();
};
