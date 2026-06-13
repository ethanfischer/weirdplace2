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
class UMenuUIComponent;
class APlayerController;
class SWidget;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	// Discrete step navigation actions. IMC_Default binds these to d-pad
	// up/down (plus W/S/arrow/left-stick up/down). Used to drive menu and
	// inventory selection movement when those UIs are open.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* NextOptionAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* PreviousOptionAction;

	// Horizontal nav actions. IMC_Default binds these to d-pad left/right
	// (plus A/D/arrow/left-stick left/right). Used for slider adjustment in
	// the settings menu and column navigation in inventory grids.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* NavigateLeftAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* NavigateRightAction;

	// Back / cancel action. Gamepad B button + Backspace. When the pause menu
	// is open on a sub-page, returns to the Pause page; when on the Pause page,
	// closes the menu. When inventory is open, closes it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* BackAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu", meta = (AllowPrivateAccess = "true"))
	UMenuUIComponent* MenuUIComponent;

public:
	// Override the rotation entry points so sensitivity scaling applies to ALL
	// callers (C++ Enhanced Input handler, the BP IA_Look event graph, plugins,
	// future cinematics, etc.) — not just our own HandleLookInput. This is the
	// cleanest place to apply the scale because both APawn callers funnel here.
	virtual void AddControllerYawInput(float Val) override;
	virtual void AddControllerPitchInput(float Val) override;

private:
	float ComputeGamepadLookScale() const;
	float ComputeMouseLookScale() const;
	bool IsGamepadLookActive() const;

#if PLATFORM_LINUX
	// Stop SDL3 per-window text input on every Slate top-level window. UE 5.7's
	// LinuxWindow.cpp turns it on at window creation and never turns it off,
	// which makes Steam Deck show the on-screen keyboard.
	void StopLinuxTextInputOnAllWindows();
#endif

public:

	// --- Input Handlers ---

	void HandleMoveInput(const FInputActionValue& Value);
	void HandleJumpStarted();
	void HandleJumpCompleted();
	void HandleInteractTriggered();
	void HandleInteractCompleted();
	void HandleShowInventory();
	void HandleShowInventoryCompleted();
	void HandleShowMenu();
	void HandleShowMenuCompleted();
	void HandleNextOption();
	void HandlePreviousOption();
	void HandleNavigateLeft();
	void HandleNavigateRight();
	void HandleBack();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Menu")
	UMenuUIComponent* GetMenuUIComponent() const { return MenuUIComponent; }

	// Dev: teleport Seneca to her smoking spot and start the smoking anim.
	// Doesn't touch CurrentState or other quest flags. Type `SkipToSmoking` in PIE console.
	UFUNCTION(Exec) void SkipToSmoking();


	// --- Interaction System ---

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RaycastInteractableCheck(AActor*& OutHitActor, bool& bDidHitInteractable);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Camera")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	// Accessors for E2E test input injection.
	UInputAction* GetInteractAction() const { return InteractAction; }
	UInputAction* GetInventoryAction() const { return InventoryAction; }
	UInputAction* GetSettingsAction() const { return SettingsAction; }
	UInputAction* GetNextOptionAction() const { return NextOptionAction; }
	UInputAction* GetPreviousOptionAction() const { return PreviousOptionAction; }
	UInputAction* GetNavigateLeftAction() const { return NavigateLeftAction; }
	UInputAction* GetNavigateRightAction() const { return NavigateRightAction; }
	UInputAction* GetBackAction() const { return BackAction; }

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

	// The dialogue widget currently in use (null outside dialogue). Tests read
	// the displayed speaker/body through this.
	UUI_Dialogue* GetActiveDialogueWidget() const { return UI_Dialogue; }

	// Last input device the player actually used (look/move/inspection input).
	// Drives device-specific button prompts.
	bool IsUsingGamepad() const { return bLastInputWasGamepad; }
	void SetUsingGamepad(bool bGamepad) { bLastInputWasGamepad = bGamepad; }

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
	// See IsUsingGamepad(). Defaults to keyboard until input is seen.
	bool bLastInputWasGamepad = false;

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
	bool bMenuDoOnceCompleted = false;
	bool bCreatedCrosshair = false;

	// Slate keyboard-focus tracing (Steam Deck on-screen keyboard diagnosis)
	TWeakPtr<SWidget> LastFocusedWidget;
	bool bLoggedInitialFocus = false;


	// Watches every app-wide input event to keep bLastInputWasGamepad correct
	// the moment any input arrives — not just during active look/rotate.
	TSharedPtr<class IInputProcessor> InputDeviceTracker;

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
