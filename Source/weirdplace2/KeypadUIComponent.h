#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KeypadUIComponent.generated.h"

class AKeypadUIActor;
class USoundBase;

// Fired once the player has entered all the digits. Param is the entered code as
// a string (e.g. "4729"). Return true if accepted (the receiver unlocks/opens and
// the component closes the keypad), false if wrong (the component plays the deny
// sound and closes). Mirrors FInventoryGiveDelegate's accept/reject contract.
DECLARE_DELEGATE_RetVal_OneParam(bool, FKeypadSubmitDelegate, const FString&);

UENUM(BlueprintType)
enum class EKeypadUIState : uint8
{
	Closed,
	Opening,
	Open,
	Closing
};

// Drives the world-space numpad UI (AKeypadUIActor). Owned by AMyCharacter like
// UInventoryUIComponent. A locked door opens it via OpenForCode and gets the
// entered code back through the submit delegate. Navigation/confirm/back are
// routed in from AFirstPersonCharacter while the keypad is open.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UKeypadUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKeypadUIComponent();

	// Open the keypad for a code of the given length, binding the submit delegate.
	void OpenForCode(int32 InCodeLength, const FKeypadSubmitDelegate& InDelegate);

	// Start closing the keypad (non-blocking animation).
	UFUNCTION(BlueprintCallable, Category = "Keypad UI")
	void CloseKeypadUI();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Keypad UI")
	bool IsKeypadOpen() const { return CurrentState == EKeypadUIState::Open || CurrentState == EKeypadUIState::Opening; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Keypad UI")
	bool IsKeypadFullyOpen() const { return CurrentState == EKeypadUIState::Open; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Keypad UI")
	bool IsKeypadFullyClosed() const { return CurrentState == EKeypadUIState::Closed; }

	// Grid navigation (routed from the character's Enhanced Input handlers).
	void NavigateLeft();
	void NavigateRight();
	void NavigateNext();      // down a row
	void NavigatePrevious();  // up a row

	// Interact (E) confirms the highlighted digit, appending it to the code.
	void PressSelectedDigit();

	// --- Test seams (mirror the inventory's SetSelectedIndexForTest) ---
	bool SetSelectedDigitForTest(int32 CellIndex);
	int32 GetSelectedDigit() const { return SelectedCell; }
	FString GetEnteredCodeForTest() const { return EnteredCode; }
	int32 GetDenySoundPlayCount() const { return DenySoundPlayCount; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad UI|Setup")
	TSubclassOf<AKeypadUIActor> KeypadUIActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad UI|Position")
	float KeypadDistance = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad UI|Position")
	float VerticalOffset = -15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad UI|Animation")
	float AnimationDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad UI|Animation")
	float AnimationDropDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad UI|Audio")
	USoundBase* MenuOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad UI|Audio")
	USoundBase* MenuCloseSound;

	// Beep played when a digit is entered.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad UI|Audio")
	USoundBase* MenuItemSelectedSound;

	// Buzzer played on a wrong code.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypad UI|Audio")
	USoundBase* DenySound;

private:
	UPROPERTY()
	EKeypadUIState CurrentState = EKeypadUIState::Closed;

	float AnimationProgress = 0.0f;

	int32 SelectedCell = 0;   // 0..8 over the fixed 3x3 grid
	int32 CodeLength = 4;
	FString EnteredCode;

	FKeypadSubmitDelegate SubmitDelegate;

	int32 DenySoundPlayCount = 0;

	FVector StoredUIPosition;
	FRotator StoredUIRotation;

	UPROPERTY()
	AKeypadUIActor* KeypadUIActor;

	void SpawnKeypadUIActor();
	void DestroyKeypadUIActor();
	void UpdateKeypadPosition();

	void BindCloseInput();
	void UnbindCloseInput();

	void FreezePlayerMovement();
	void UnfreezePlayerMovement();

	// Move the selected cell by (DeltaCol, DeltaRow), clamped to the 3x3 grid.
	void StepSelection(int32 DeltaCol, int32 DeltaRow);

	// Validate the entered code via the delegate; deny + close on reject, close on accept.
	void SubmitCode();
};
