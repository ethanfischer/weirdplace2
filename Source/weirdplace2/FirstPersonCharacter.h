#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
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
class UStormFogComponent;
class UFootstepComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UWeirdplaceGameUserSettings;
class UMenuUIComponent;
class APlayerController;
class SWidget;
class UInventoryComponent;
class UInventoryUIComponent;
class UKeypadUIComponent;

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

UENUM(BlueprintType)
enum class EPlayerActivityState : uint8
{
	FreeRoaming            UMETA(DisplayName = "Free Roaming"),
	Interacting            UMETA(DisplayName = "Interacting"),
	InSimpleDialogue       UMETA(DisplayName = "In Simple Dialogue"),
	InDialogue                              UMETA(DisplayName = "In Dialogue")
};

UCLASS(Blueprintable)
class WEIRDPLACE2_API AFirstPersonCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AFirstPersonCharacter();

	void SetCanInteract(bool value);
	bool GetCanInteract() const { return CanInteract; }

	void SetActivityState(EPlayerActivityState NewState);
	EPlayerActivityState GetActivityState() const { return ActivityState; }
	bool IsInAnyDialogue() const;
	bool IsDialogueCooldownActive() const;

	// Enter an interaction "hold": freeze movement (and look if bFreezeLook),
	// disable environment interaction, set Interacting, and ensure the player
	// controller has an InputComponent so the caller can bind its own exit/rotate
	// actions. Shared by PayPhone / MovieBox / InspectablePickup; each caller still
	// binds its own actions afterward. No-op without a player controller.
	void BeginInteractionHold(bool bFreezeLook);

	// Reverse of BeginInteractionHold: unfreeze movement (and look if
	// bUnfreezeLook), re-enable interaction, return to FreeRoaming.
	void EndInteractionHold(bool bUnfreezeLook);

	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;

	// Locks movie collection (called by Seneca when checkout begins)
	void LockMovieCollection();
	bool IsMovieCollectionLocked() const { return bMovieCollectionLocked; }

	// One-time "put back" control hint: shown only until the player's first
	// completed movie interaction — collecting one or putting one back.
	bool HasInteractedWithMovie() const { return bHasInteractedWithMovie; }
	void MarkMovieInteraction() { bHasInteractedWithMovie = true; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryUIComponent* GetInventoryUIComponent() const { return InventoryUIComponent; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Keypad")
	UKeypadUIComponent* GetKeypadUIComponent() const { return KeypadUIComponent; }

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting", meta = (AllowPrivateAccess = "true"))
	URectLightComponent* ItemHoldLightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bladder Urgency")
	UBladderUrgencyComponent* BladderUrgencyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm Fog")
	UStormFogComponent* StormFogComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	UFootstepComponent* FootstepComponent;

	// Console (~): toggle the pea-soup storm fog on/off for testing. Type "PeaSoup".
	UFUNCTION(Exec)
	void PeaSoup();

	// Console: set the fog view distance in cm live, e.g. "PeaSoupDist 80" (thicker).
	UFUNCTION(Exec)
	void PeaSoupDist(float Centimeters);

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

	UFUNCTION(BlueprintCallable, Category = "Lighting")
	void SetItemHoldLightEnabled(bool bEnabled);

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
	// --- Merged from AMyCharacter ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	bool CanInteract = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	EPlayerActivityState ActivityState = EPlayerActivityState::FreeRoaming;

	double LastDialogueEndTime = -TNumericLimits<double>::Max();

	bool bMovieCollectionLocked = false;

	// Set true on the player's first completed movie interaction (collect or
	// put-back); suppresses the put-back prompt on every subsequent inspection.
	// Runtime-only (resets each session), like bMovieCollectionLocked.
	bool bHasInteractedWithMovie = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryComponent* InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryUIComponent* InventoryUIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Keypad", meta = (AllowPrivateAccess = "true"))
	UKeypadUIComponent* KeypadUIComponent;

	// --- End merged from AMyCharacter ---

	// See IsUsingGamepad(). Defaults to keyboard until input is seen.
	bool bLastInputWasGamepad = false;

	// --- Item Notification ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* ItemNotificationMesh;

	// Dynamically spawned mesh components for stacked item notifications
	UPROPERTY()
	TArray<UStaticMeshComponent*> StackNotificationMeshes;

	// Self-illumination overlay (M_ItemDarkGlow) applied to notification popups so
	// received items (e.g. Seneca's key) read in the game's dark areas, matching
	// held items / inspected pickups. Emissive-only — never lights the environment.
	UPROPERTY()
	UMaterialInterface* NotificationGlowMaterial = nullptr;

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
