#pragma once

#include "CoreMinimal.h"
#include "MyCharacter.h"
#include "InputActionValue.h"
#include "FirstPersonCharacter.generated.h"

class UCameraComponent;
class UCrosshairWidget;
class UUI_Dialogue;
class UDlgDialogue;
class UDlgContext;
class UWidgetComponent;
class UInputAction;
class UInputMappingContext;
class URectLightComponent;
class UBladderUrgencyComponent;

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

	// --- Inventory UI Widget Component (set in Blueprint) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	UWidgetComponent* InventoryUIWidgetComponent;

	// --- Dialogue ---

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	UUI_Dialogue* UI_Dialogue;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	UDlgContext* DialogueContext;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool IsInDialogue = false;

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

	// --- Interaction System ---

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RaycastInteractableCheck(AActor*& OutHitActor, bool& bDidHitInteractable);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Camera")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintCallable, Category = "Lighting")
	void SetInventoryFlashlightEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lighting")
	bool IsInventoryFlashlightEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Lighting")
	void SetInventoryFlashlightSize(float Width, float Height);

	// --- Dialogue System ---

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void StartDialogueWithNPC(UDlgDialogue* Dialogue, UObject* NPC);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void SelectDialogueOption(int32 OptionIndex);

	// Simple text-based dialogue (no DlgContext)
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void StartSimpleDialogue(const FText& SpeakerName, const TArray<FText>& Lines, UObject* NPC);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void AdvanceSimpleDialogue();

	// Multi-speaker dialogue (each line has its own speaker)
	void StartSimpleDialogueMultiSpeaker(const TArray<FSimpleDialogueLine>& Lines, UObject* NPC);
	void AdvanceMultiSpeakerDialogue();

	// Fires whenever a multi-speaker dialogue line is displayed, carrying the line index
	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueLineShown OnDialogueLineShown;

	// When true, the next multi-speaker advance is consumed without progressing.
	// OnDialogueLineShown broadcasts with the CURRENT index so listeners can act.
	bool bBlockNextMultiSpeakerAdvance = false;

private:
	// DoOnce state tracking
	bool bInteractDoOnceCompleted = false;
	bool bInventoryDoOnceCompleted = false;
	bool bCreatedCrosshair = false;

	// The NPC we're currently in dialogue with (for end-of-dialogue callbacks)
	UPROPERTY()
	UObject* CurrentDialogueNPC = nullptr;

	// Simple dialogue state
	TArray<FText> SimpleDialogueLines;
	int32 SimpleDialogueLineIndex = 0;
	FText SimpleDialogueSpeaker;
	bool bIsSimpleDialogue = false;

	// Multi-speaker dialogue state
	TArray<FSimpleDialogueLine> MultiSpeakerLines;
	int32 MultiSpeakerLineIndex = 0;
	bool bIsMultiSpeakerDialogue = false;
};
