#pragma once

#include "CoreMinimal.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "InputActionValue.h"
#include "FirstPersonCharacter.generated.h"

class UCameraComponent;
class UCrosshairWidget;
class UUI_Dialogue;
class UInputAction;
class UInputMappingContext;
class URectLightComponent;
class UBladderUrgencyComponent;
class UStaticMeshComponent;
class UWeirdplaceGameUserSettings;
class USettingsUIComponent;
class APlayerController;

USTRUCT()
struct FSimpleDialogueLine
{
	GENERATED_BODY()

	UPROPERTY()
	FText Speaker;

	UPROPERTY()
	FText Text;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueLineShown, int32, LineIndex);

UCLASS(Blueprintable)
class WEIRDPLACE2_API AFirstPersonCharacter : public AMyCharacter
{
	GENERATED_BODY()

public:
	AFirstPersonCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting", meta = (AllowPrivateAccess = "true"))
	URectLightComponent* InventoryFlashlightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bladder Urgency")
	UBladderUrgencyComponent* BladderUrgencyComponent;

	// --- Crosshair Widget ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UCrosshairWidget> CrosshairWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	UCrosshairWidget* CrosshairWidget;

	// --- Dialogue ---

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	UUI_Dialogue* UI_Dialogue;

	// --- Interaction ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionDistance = 500.0f;

	// --- Enhanced Input Actions (set in Blueprint) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* InteractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* InventoryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* SettingsAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	USettingsUIComponent* SettingsUIComponent;

public:
	// --- Input Handlers ---

	void HandleLookInput(const FInputActionValue& Value);
	void HandleMoveInput(const FInputActionValue& Value);
	void HandleJumpStarted();
	void HandleJumpCompleted();
	void HandleInteractTriggered();
	void HandleInteractCompleted();
	void HandleShowInventory();
	void HandleShowInventoryCompleted();
	void HandleShowSettings();
	void HandleShowSettingsCompleted();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Settings")
	USettingsUIComponent* GetSettingsUIComponent() const { return SettingsUIComponent; }

	// --- Interaction System ---

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RaycastInteractableCheck(AActor*& OutHitActor, bool& bDidHitInteractable);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Camera")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	// Accessors for E2E test input injection.
	UInputAction* GetInteractAction() const { return InteractAction; }
	UInputAction* GetInventoryAction() const { return InventoryAction; }

	UFUNCTION(BlueprintCallable, Category = "Lighting")
	void SetInventoryFlashlightEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lighting")
	bool IsInventoryFlashlightEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Lighting")
	void SetInventoryFlashlightSize(float Width, float Height);

	// --- Item Notification ---

	// Shows the item's 3D mesh in front of the player camera for 3 seconds
	void ShowItemNotification(const FInventoryItemData& ItemData, const FRotator& InitialRotation = FRotator::ZeroRotator);

	// Shows multiple items stacked vertically in front of the camera (no auto-dismiss timer)
	void ShowItemNotificationStack(const TArray<FInventoryItemData>& Items, const FRotator& ItemRotation = FRotator::ZeroRotator);

	bool IsItemNotificationVisible() const;

	// --- Dialogue System ---

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void StartSimpleDialogue(const FText& SpeakerName, const TArray<FText>& Lines, UObject* NPC);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void AdvanceSimpleDialogue();

	void StartDialogue(const TArray<FSimpleDialogueLine>& Lines, UObject* NPC);
	void AdvanceDialogue();

	// Fires whenever a dialogue line is displayed, carrying the line index
	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueLineShown OnDialogueLineShown;

	// When true, the next dialogue advance is consumed without progressing.
	// OnDialogueLineShown broadcasts with the CURRENT index so listeners can act.
	bool bBlockNextDialogueAdvance = false;

private:
	// --- Item Notification ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* ItemNotificationMesh;

	// Dynamically spawned mesh components for stacked item notifications
	UPROPERTY()
	TArray<UStaticMeshComponent*> StackNotificationMeshes;

	void ClearItemNotificationStack();

	FTimerHandle ItemNotificationTimerHandle;

	// DoOnce state tracking
	bool bInteractDoOnceCompleted = false;
	bool bInventoryDoOnceCompleted = false;
	bool bSettingsDoOnceCompleted = false;
	bool bCreatedCrosshair = false;

	// Cached for gamepad-aware look input scaling. Resolved in BeginPlay.
	UPROPERTY()
	TObjectPtr<APlayerController> CachedPlayerController;

	UPROPERTY()
	TObjectPtr<UWeirdplaceGameUserSettings> CachedSettings;

	// The NPC we're currently in dialogue with (for end-of-dialogue callbacks)
	UPROPERTY()
	UObject* CurrentDialogueNPC = nullptr;

	// Simple dialogue state
	TArray<FText> SimpleDialogueLines;
	int32 SimpleDialogueLineIndex = 0;
	FText SimpleDialogueSpeaker;

	// Dialogue state (lines with per-line speaker)
	TArray<FSimpleDialogueLine> DialogueLines;
	int32 DialogueLineIndex = 0;

	// --- Interaction helpers (used by RaycastInteractableCheck) ---

	bool IsLineOfSightClearToActor(const FVector& CameraLocation, AActor* Target, const TArray<AActor*>& AdditionalIgnoreActors) const;
	bool IsWithinNPCInteractionRange(AActor* Target) const;
};
