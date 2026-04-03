#pragma once

#include "CoreMinimal.h"
class APropActor;
#include "Interactable.h"
#include "DialogueWidgetProvider.h"
#include "DlgSystem/DlgDialogueParticipant.h"
#include "GameFramework/Actor.h"
#include "Seneca.generated.h"

class UWidgetComponent;
class UUI_Dialogue;
class UDlgContext;
class UStaticMesh;
class UTexture2D;
class UChildActorComponent;
class UAnimSequenceBase;
class ADoor;
class AFirstPersonCharacter;
class UInventoryComponent;

UENUM(BlueprintType)
enum class ESenecaState : uint8
{
	WaitingForMovies,           // "Buy 3 movies first"
	WaitingForMoviePurchase,    // Player must give each movie to Seneca
	WaitingForMoney,            // Price quoted; player needs to find money
	ReadyToGiveKey,             // "Nice picks, here's the key"
	GaveKey,                    // "Go use the bathroom outside"
	Smoking,                    // "Door's busted, use employee bathroom"
	AtEmployeeBathroom,         // "Here you go" + unlocks door
	Done                        // No more dialogue
};

UCLASS()
class WEIRDPLACE2_API ASeneca : public AActor, public IInteractable, public IDlgDialogueParticipant, public IDialogueWidgetProvider
{
	GENERATED_BODY()

public:
	ASeneca();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	// IInteractable implementation
	virtual void Interact_Implementation() override;

	// IDialogueWidgetProvider implementation
	virtual UUI_Dialogue* GetDialogueWidget() const override;

	// IDlgDialogueParticipant implementation
	virtual FName GetParticipantName_Implementation() const override;
	virtual FText GetParticipantDisplayName_Implementation(FName ActiveSpeaker) const override;
	virtual ETextGender GetParticipantGender_Implementation() const override;
	virtual UTexture2D* GetParticipantIcon_Implementation(FName ActiveSpeaker, FName ActiveSpeakerState) const override;
	virtual bool CheckCondition_Implementation(const UDlgContext* Context, FName ConditionName) const override;
	virtual float GetFloatValue_Implementation(FName ValueName) const override;
	virtual int32 GetIntValue_Implementation(FName ValueName) const override;
	virtual bool GetBoolValue_Implementation(FName ValueName) const override;
	virtual FName GetNameValue_Implementation(FName ValueName) const override;
	virtual bool OnDialogueEvent_Implementation(UDlgContext* Context, FName EventName) override;
	virtual bool ModifyFloatValue_Implementation(FName ValueName, bool bDelta, float Value) override;
	virtual bool ModifyIntValue_Implementation(FName ValueName, bool bDelta, int32 Value) override;
	virtual bool ModifyBoolValue_Implementation(FName ValueName, bool bNewValue) override;
	virtual bool ModifyNameValue_Implementation(FName ValueName, FName NameValue) override;

	// Widget component hosting the dialogue UI - auto-found by name in BeginPlay
	UPROPERTY(BlueprintReadOnly, Category = "Seneca|Dialogue")
	UWidgetComponent* DialogueWidgetComponent;

	// --- Quest State ---

	// Called by OutsideBathroomDoor when the key is dropped
	void OnKeyDropped();

	// Called by FirstPersonCharacter when dialogue with Seneca ends
	void OnDialogueEnded();

	// Current quest state (read-only in editor for debugging)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seneca|Quest")
	ESenecaState CurrentState = ESenecaState::WaitingForMovies;

	// Number of movies the player has given to Seneca (read-only for debugging)
	UPROPERTY(VisibleAnywhere, Category = "Seneca|Quest")
	int32 MoviesGivenCount = 0;

	// Set once the WaitingForMovies basket beat has fully played; prevents replaying on re-enter/re-interact
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIntroDialoguePlayed = false;

	// --- Animation ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Animation")
	UAnimSequenceBase* SmokingAnimation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seneca|Animation")
	bool bIsSmoking = false;

protected:
	// --- Components (assigned in Blueprint) ---

	UPROPERTY(BlueprintReadOnly, Category = "Seneca")
	UChildActorComponent* CigaretteComp;

	// --- Dialogue per state (txt file paths relative to Content/) ---

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoviesDialoguePath = TEXT("Dialogue/WaitingForMovies.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString MovieCommentsPath = TEXT("Dialogue/MovieComments.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString MoviePurchaseDialoguePath = TEXT("Dialogue/MoviePurchase.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoneyDialoguePath = TEXT("Dialogue/WaitingForMoney.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoviePurchaseDialoguePath = TEXT("Dialogue/WaitingForMoviePurchase.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString ReadyToGiveKeyDialoguePath = TEXT("Dialogue/ReadyToGiveKey.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString GaveKeyDialoguePath = TEXT("Dialogue/GaveKey.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString SmokingDialoguePath = TEXT("Dialogue/Smoking.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString EmployeeBathroomDialoguePath = TEXT("Dialogue/EmployeeBathroom.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoviesReminderPath = TEXT("Dialogue/WaitingForMoviesReminder.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoviePurchaseReminderPath = TEXT("Dialogue/WaitingForMoviePurchaseReminder.txt");

	// --- Key ---

	// Key name given to player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	FName KeyToGive = FName("Key");

	// Static mesh for the key (used for inventory visual data)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	UStaticMesh* KeyMesh;

	// Scale override for the key in inventory/held view
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	FVector KeyScale = FVector(0.001f, 0.001f, 0.001f);

	// Thumbnail shown in inventory after the key is dropped (broken key)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	UTexture2D* KeyBrokenThumbnail;

	// Pre-placed key actor in the level — shown on beat, hidden on next E press
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	APropActor* KeyActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	int32 KeyBeatLineIndex = 0;

	// --- Quest Config ---

	// Number of movies required before Seneca gives the key
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Quest", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequiredMovieCount = 3;

	// Seconds after key drop before Seneca appears at smoking position
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Quest", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SmokingAppearDelay = 60.0f;

	// --- Position Targets (assign on level instance, these are level actor refs) ---

	// Empty actor placed at the smoking spot outside
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Positions")
	AActor* SmokingPositionTarget;

	// Empty actor placed at the employee bathroom
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Positions")
	AActor* EmployeeBathroomPositionTarget;

	// --- Door Reference (assign on level instance) ---

	// The employee bathroom door (starts locked, unlocked by Seneca)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Door")
	ADoor* EmployeeBathroomDoor;

	// --- Basket Beat Config ---

	// Pre-placed ShoppingBasket actor in the level — shown on beat, hidden on next E press
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Basket")
	APropActor* ShoppingBasketActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Basket")
	int32 BasketBeatLineIndex = 2;

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString InventoryButtonDisplayName = TEXT("Tab");

private:
	// Gives the key to the player
	void GiveKey();

	// Teleport Seneca to target actor's location/rotation
	void MoveToTarget(AActor* Target);

	// Returns the loaded dialogue lines for the current state
	const TArray<FText>* GetDialogueLinesForCurrentState() const;

	// Check if player has enough movies and update state accordingly
	void CheckMovieCount();

	// Loaded dialogue lines per state
	TMap<ESenecaState, TArray<FText>> DialogueLines;

	// Reminder lines for re-interactions within a state
	TArray<FText> WaitingForMoviesReminderLines;
	TArray<FText> WaitingForMoviePurchaseReminderLines;

	// Helper to load a single dialogue file
	void LoadDialogueFile(ESenecaState State, const FString& RelativePath);

	// Delegate listener for inventory changes
	UFUNCTION()
	void OnInventoryChanged(const TArray<FName>& CurrentItems);

	// Returns true if the player camera is facing the given world position
	bool IsPlayerLookingAt(const FVector& Position) const;

	// Returns true if the player camera is looking at Seneca
	bool IsPlayerLookingAtMe() const;

	// Deferred move: target to teleport to once player looks away
	UPROPERTY()
	AActor* PendingMoveTarget = nullptr;

	// Cached skeletal mesh for computing look-at bounds target
	UPROPERTY()
	USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

	// Tracks that the player was looking at Seneca (requires look then look-away)
	bool bWasLookingAtMe = false;

	// Timer for delayed appearance at smoking position
	FTimerHandle SmokingAppearTimerHandle;

	// True while waiting for player to look away so Seneca can appear
	bool bWaitingToAppear = false;

	// Called when the smoking appear delay expires
	void OnSmokingDelayComplete();

	// --- Basket Beat ---

	void StartWaitingForMoviesDialogue(AFirstPersonCharacter* FPChar);

	UFUNCTION()
	void OnBasketDialogueLineShown(int32 LineIndex);

	bool bBasketBeatArmed = false;

	// --- Key Beat ---

	void StartReadyToGiveKeyDialogue(AFirstPersonCharacter* FPChar);

	UFUNCTION()
	void OnKeyDialogueLineShown(int32 LineIndex);

	bool bKeyBeatArmed = false;

	// --- Movie Purchase Beat ---

	// Per-movie comment lookup (key = DataTable row name, e.g. "BLADE-RUNNER")
	TMap<FName, FText> MovieComments;
	FText FallbackMovieComment;

	void LoadMovieComments();
	void HandleMovieGive(AFirstPersonCharacter* FPChar, UInventoryComponent* Inventory, FName MovieID);
	void StartMoviePurchaseDialogue(AFirstPersonCharacter* FPChar);
};
