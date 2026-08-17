#pragma once

#include "CoreMinimal.h"
class APropActor;
#include "Interactable.h"
#include "DialogueWidgetProvider.h"
#include "GameFramework/Actor.h"
#include "Inventory.h"
#include "DialogueScript.h"
#include "Seneca.generated.h"

class UWidgetComponent;
class UStaticMeshComponent;
class UUI_Dialogue;
class UItemDefinition;
class UTexture2D;
class UChildActorComponent;
class UAnimSequenceBase;
class ADoor;
class AFirstPersonCharacter;
class UInventoryComponent;
class USpawnerActorComponent;

UENUM(BlueprintType)
enum class ESenecaState : uint8
{
	WaitingForMovies,           // "Buy 3 movies first"
	WaitingForMoviePurchase,    // Player must give each movie to Seneca
	WaitingForMoney,            // Price quoted; player needs to find money
	WaitingForBlankTape,        // "I need a blank tape too"
	AwaitingTapeBurn,           // "find me later"
	ReadyToGiveCombinedTape,    // External trigger fired; next interact hands tape over
	ReadyToGiveKey,             // "Nice picks, here's the key"
	GaveKey,                    // "Go use the bathroom outside"
	Smoking,                    // Cosmetic smoking beat (no story effect)
	Done                        // No more dialogue
};

UCLASS()
class WEIRDPLACE2_API ASeneca : public AActor, public IInteractable, public IDialogueWidgetProvider
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

	// Widget component hosting the dialogue UI - auto-found by name in BeginPlay
	UPROPERTY(BlueprintReadOnly, Category = "Seneca|Dialogue")
	UWidgetComponent* DialogueWidgetComponent;

	// --- Quest State ---

	// Called by OutsideBathroomDoor when the key is dropped
	void OnKeyDropped();

	// Test-only: clear the SmokingAppearDelay timer and jump straight to
	// OnSmokingDelayComplete so E2E tests don't have to wait 60 seconds.
	void FastForwardSmokingAppear();

	// Dev-only: teleport to SmokingPositionTarget and start the smoking anim NOW,
	// without touching CurrentState or other quest flags.
	void ForceSmokingAppearance();

	// External trigger: combined-tape compilation finished. Only transitions
	// AwaitingTapeBurn -> ReadyToGiveCombinedTape; otherwise logs + returns.
	void GiveCombinedTape();

	// Called by FirstPersonCharacter when dialogue with Seneca ends
	void OnDialogueEnded();

	// Single source of truth for the lines spoken in a given state: copies the
	// loaded lines for `State` and, for Smoking once the player has seen the
	// tornado warning (UStorySubsystem SeenTornadoWarning), appends the
	// shelter tip. Both Interact_Implementation and the E2E hook call this.
	void BuildEffectiveDialogueLines(ESenecaState State, TArray<FText>& Out) const;

	// Current quest state (read-only in editor for debugging)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seneca|Quest")
	ESenecaState CurrentState = ESenecaState::WaitingForMovies;

	// Number of movies the player has given to Seneca (read-only for debugging)
	UPROPERTY(VisibleAnywhere, Category = "Seneca|Quest")
	int32 MoviesGivenCount = 0;

	// Set once the WaitingForMovies basket beat has fully played; prevents replaying on re-enter/re-interact
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIntroDialoguePlayed = false;

	// Test access: returns the pre-placed key prop actor (shown during the key beat).
	APropActor* GetKeyActor() const { return KeyActor; }

	// --- Animation ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Animation")
	UAnimSequenceBase* SmokingAnimation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seneca|Animation")
	bool bIsSmoking = false;

protected:
	// --- Components (assigned in Blueprint) ---

	UPROPERTY(BlueprintReadOnly, Category = "Seneca")
	UChildActorComponent* CigaretteComp;

	// --- Dialogue (sectioned file parsed by FDialogueScript; see scripts/dq.py) ---

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString DialogueFilePath = TEXT("Dialogue/Seneca.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString MovieCommentsPath = TEXT("Dialogue/MovieComments.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoviesSection = TEXT("WaitingForMovies");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString MoviePurchaseSection = TEXT("MoviePurchase");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoneySection = TEXT("WaitingForMoney");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForBlankTapeSection = TEXT("WaitingForBlankTape");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForBlankTapeReminderSection = TEXT("WaitingForBlankTapeReminder");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString AwaitingTapeBurnSection = TEXT("AwaitingTapeBurn");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString ReadyToGiveCombinedTapeSection = TEXT("ReadyToGiveCombinedTape");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoviePurchaseSection = TEXT("WaitingForMoviePurchase");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString ReadyToGiveKeySection = TEXT("ReadyToGiveKey");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString GaveKeySection = TEXT("GaveKey");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString SmokingSection = TEXT("Smoking");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoviesReminderSection = TEXT("WaitingForMoviesReminder");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoviePurchaseReminderSection = TEXT("WaitingForMoviePurchaseReminder");

	// --- Blank Tape Beat ---

	// Level-placed actor that owns the USpawnerActorComponent (assign on level instance).
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Seneca|BlankTape")
	AActor* MovieSpawnerActor = nullptr;

	// Combined tape data asset handed over after the burn completes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|BlankTape")
	UItemDefinition* CombinedTapeDef = nullptr;

	// --- Key ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	UItemDefinition* KeyDef;

	// Pre-placed key actor in the level — shown on beat, hidden on next E press
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	APropActor* KeyActor;

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

	// --- Counter Stack ---

	// Position marker for where the first movie goes on the counter (assign on level instance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Counter")
	AActor* CounterStackPosition;

	// Vertical offset per stacked movie (Unreal units, applied along marker's up vector)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Counter")
	float MovieStackHeight = 4.0f;

	// Relative rotation applied to each spawned movie mesh (use to correct authoring-axis mismatch)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Counter")
	FRotator MovieRelativeRotation = FRotator::ZeroRotator;

	// Pre-placed ShoppingBasket actor in the level. The basket-give beat is
	// gone (inventory works from spawn); the prop stays hidden at BeginPlay
	// until it's re-dressed or deleted in the editor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Basket")
	APropActor* ShoppingBasketActor;

private:
	// Gives the key to the player and shows the item notification on success.
	// Skips the notification (logs + bails) on any failure path.
	void GiveKey(class AFirstPersonCharacter* FPChar);

	// Teleport Seneca to target actor's location/rotation
	void MoveToTarget(AActor* Target);

	// Loaded dialogue lines per state
	TMap<ESenecaState, TArray<FText>> DialogueLines;

	// Per-state action cues parsed from `[ActionName]` lines in dialogue files.
	// Key = display line index that the action follows; Value = action name.
	TMap<ESenecaState, TMap<int32, FString>> LineActions;

	// Returns the action name for a given line index in the current state, or empty string.
	FString GetActionForLine(ESenecaState State, int32 LineIndex) const;

	// Movies captured during WaitingForMoviePurchase so they can be returned in ReadyToGiveKey.
	UPROPERTY()
	TArray<FInventoryItemData> TakenMovies;

	// Spawned visual props for movies stacked on the counter
	UPROPERTY()
	TArray<AActor*> CounterMovieActors;

	void PlaceMovieOnCounter(const FInventoryItemData& MovieData);
	void ClearCounterMovies();

	// Reminder lines for re-interactions within a state
	TArray<FText> WaitingForMoviesReminderLines;
	TArray<FText> WaitingForMoviePurchaseReminderLines;

	// Parsed sectioned dialogue file (DialogueFilePath)
	FDialogueScript DialogueScript;

	// Copies a section of DialogueScript into DialogueLines/LineActions for a state
	void LoadDialogueSection(ESenecaState State, const FString& SectionName);

	// Copies a section's text into a flat line array (reminder lines)
	void LoadReminderSection(const FString& SectionName, TArray<FText>& OutLines);

	// Returns true if the player camera is facing the given world position
	bool IsPlayerLookingAt(const FVector& Position) const;

	// Cached skeletal mesh for computing look-at bounds target
	UPROPERTY()
	USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

	// Returns the SkeletalMeshComponent on this actor whose mesh skeleton matches
	// SmokingAnimation's skeleton (i.e. the MetaHuman body mesh, not face/hair).
	USkeletalMeshComponent* FindBodyMesh() const;

public:
	void StartSmokingAnim();
private:

	// Timer for delayed appearance at smoking position
	FTimerHandle SmokingAppearTimerHandle;

	// True while waiting for player to look away so Seneca can appear
	bool bWaitingToAppear = false;

	// Called when the smoking appear delay expires
	void OnSmokingDelayComplete();

	void StartWaitingForMoviesDialogue(AFirstPersonCharacter* FPChar);

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

	// Returns the first inventory item that is a movie (not a fixed tool/quest
	// item), or NAME_None. Movies are handed to Seneca one per interact.
	static FName FindFirstMovie(UInventoryComponent* Inventory);

	// True if ItemID is a movie (any item that isn't a fixed tool/quest item).
	static bool IsMovieItem(FName ItemID);

	// Open the inventory in give-mode bound to OnInventoryItemOffered.
	void OpenGiveForOffer(AFirstPersonCharacter* MyCharacter);

	// Inventory give-mode callback: validates the offered item for the current
	// give-state, consumes + advances on accept (returns true), else false.
	bool OnInventoryItemOffered(FName ItemID);

	// --- Blank Tape Beat ---

	UPROPERTY()
	USpawnerActorComponent* CachedMovieSpawner = nullptr;

	TArray<FText> WaitingForBlankTapeReminderLines;

	void StartWaitingForBlankTapeDialogue(AFirstPersonCharacter* FPChar);
	void HandleBlankTapeGive(AFirstPersonCharacter* FPChar, UInventoryComponent* Inventory, FName BlankTapeID);
	void StartAwaitingTapeBurnDialogue(AFirstPersonCharacter* FPChar);

	// --- Combined Tape Beat ---

	void StartReadyToGiveCombinedTapeDialogue(AFirstPersonCharacter* FPChar);

	UFUNCTION()
	void OnCombinedTapeDialogueLineShown(int32 LineIndex);

	bool bCombinedTapeBeatArmed = false;
};
