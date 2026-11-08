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
class UUI_Dialogue;
class UItemDefinition;
class UTexture2D;
class ADoor;
class AFirstPersonCharacter;
class UInventoryComponent;
enum class EStoryFlag : uint8;
class APiano;
class USpawnerActorComponent;

UENUM(BlueprintType)
enum class ESenecaState : uint8
{
	WaitingForMovies,           // Czech intro, then the reminder on loop until Rick translates
	WaitingForMoviePurchase,    // Player must give each movie to Seneca
	WaitingForMoney,            // Price quoted; player needs to find money
	WaitingForBlankTape,        // "I need a blank tape too"; takes it only after Rick translates
	AwaitingTapeBurn,           // "find me later"
	ReadyToGiveCombinedTape,    // External trigger fired; next interact hands tape over
	ReadyToGiveKey,             // "Nice picks, here's the key"
	GaveKey,                    // "Go use the bathroom outside"
	AtPiano,                    // Oasis: standing at the piano (EnteredOasis); "Hey. Where have you been?"
	Done                        // Gone: left the store when the key broke, until the oasis
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

	// Finds, positions and billboards the plate; DialogueWidgetComponent mirrors its widget.
	UPROPERTY(VisibleAnywhere, Category = "Seneca|Dialogue")
	class UDialoguePlateComponent* DialoguePlate;

	// Player-only blocking capsule so the player can't walk into the NPC.
	UPROPERTY(VisibleAnywhere, Category = "Seneca|Dialogue")
	class UNpcPersonalSpaceComponent* PersonalSpace;

	// --- Quest State ---

	// Called by OutsideBathroomDoor when the key is dropped: Seneca leaves the
	// store (hidden below the world) and next appears at the oasis piano.
	void OnKeyDropped();

	// Hidden below the world between the key breaking and the oasis (tests).
	bool IsGone() const { return GetActorLocation().Z < GoneZ * 0.5; }

	// External trigger: combined-tape compilation finished. Only transitions
	// AwaitingTapeBurn -> ReadyToGiveCombinedTape; otherwise logs + returns.
	void GiveCombinedTape();

	// Called by FirstPersonCharacter when dialogue with Seneca ends
	void OnDialogueEnded();

	// Copies the loaded lines for `State` into Out.
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

	// Rick has translated the blank-tape request; until then Seneca won't take the tape.
	UPROPERTY(VisibleAnywhere, Category = "Seneca|Quest")
	bool bBlankTapeRequestTranslated = false;

	// Rick finished translating Seneca's open request. After the intro he
	// starts taking movies (WaitingForMovies -> WaitingForMoviePurchase);
	// after the blank-tape request he'll take the tape.
	void OnRickTranslated();

	// Test access: returns the pre-placed key prop actor (shown during the key beat).
	APropActor* GetKeyActor() const { return KeyActor; }

protected:
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
	FString WaitingForMoviesReminderSection = TEXT("WaitingForMoviesReminder");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoviePurchaseReminderSection = TEXT("WaitingForMoviePurchaseReminder");

	// Oasis: what Seneca says at the piano.
	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString PianoSection = TEXT("Piano");

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

	// OnKeyDropped parks Seneca this far below the world (visibility cycling
	// on a MetaHuman breaks the groom hair on re-show).
	static constexpr double GoneZ = -100000.0;

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

	// Cached skeletal mesh for computing look-at bounds target
	UPROPERTY()
	USkeletalMeshComponent* CachedSkeletalMesh = nullptr;

	void StartWaitingForMoviesDialogue(AFirstPersonCharacter* FPChar);

	// --- Key Beat ---

	void StartReadyToGiveKeyDialogue(AFirstPersonCharacter* FPChar);

	// EnteredOasis -> GoToPiano.
	void OnStoryFlagChanged(EStoryFlag Flag, bool bValue);

	// Stand at the oasis piano's PianistSpot.
	void GoToPiano();

	// The piano he's at (AtPiano). Its music pauses while the player talks to
	// him and resumes from the same spot when they stop (Tick).
	TWeakObjectPtr<APiano> AtPianoPiano;
	bool bPianoPausedForTalk = false;

	// Line-cue handler for the ReadyToGiveKey ([Give key]) dialogue: arms on
	// the cue line, executes on the next press.
	UFUNCTION()
	void OnKeyDialogueLineShown(int32 LineIndex);

	// Builds speaker-tagged lines and starts the dialogue with the cue handler bound.
	void StartCuedDialogue(AFirstPersonCharacter* FPChar, const TArray<FText>& Lines);

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
