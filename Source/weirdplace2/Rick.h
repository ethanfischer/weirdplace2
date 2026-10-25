#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FirstPersonCharacter.h"
#include "Interactable.h"
#include "DialogueWidgetProvider.h"
#include "DialogueScript.h"
#include "Rick.generated.h"

class UWidgetComponent;
class UItemDefinition;
class ASeneca;
class UUI_Dialogue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRickDialogueEnded);

UCLASS()
class WEIRDPLACE2_API ARick : public AActor, public IInteractable, public IDialogueWidgetProvider
{
	GENERATED_BODY()

public:
	ARick();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	// Start the car ride dialogue on the player character
	void StartDialogue();

	// Called by FirstPersonCharacter when dialogue ends
	void OnDialogueEnded();

	// IInteractable
	virtual void Interact_Implementation() override;
	virtual bool CanInteract() override { return bInteractionEnabled; }

	// Car ride end: Rick stays in the driver's seat and can't be talked to
	void SetInteractionEnabled(bool bEnabled) { bInteractionEnabled = bEnabled; }

	// IDialogueWidgetProvider
	virtual UUI_Dialogue* GetDialogueWidget() const override;

	// Reveal Rick at his outside-store position (called by CarRideComponent after fade)
	void AppearOutside();

	// Move Rick to InsidePositionTarget. Normally deferred via Tick until neither
	// his current spot nor the counter spot is in view (the move must never be
	// seen); tests may call directly.
	void TeleportInside();

	// True once Rick stands at InsidePositionTarget.
	bool IsInside() const { return bInside; }

	// Where AppearOutside parks the car (OutsidePositionTarget + CarActorOffset,
	// facing OutsidePositionTarget). Returns false if the target is unassigned.
	bool GetCarParkingTransform(FTransform& OutTransform) const;

	// Storm beat: remove Rick from the world until a much-later beat brings him
	// back. MetaHuman grooms break if root visibility is toggled, so this
	// teleports him far below the level and disables collision instead.
	void StashForStorm();

	// Restore Rick to where he stood before the storm stash. (No caller yet —
	// the "much later" return beat will use this.)
	void ReturnFromStorm();

private:
	bool bInteractionEnabled = true;

public:

	// Delegate fired when dialogue ends (CarRideComponent binds to this)
	UPROPERTY(BlueprintAssignable, Category = "Rick|Dialogue")
	FOnRickDialogueEnded OnRickDialogueEnded;

	// Widget component hosting the dialogue UI - auto-found by name in BeginPlay
	UPROPERTY(BlueprintReadOnly, Category = "Rick|Dialogue")
	UWidgetComponent* DialogueWidgetComponent;

	// Finds, positions and billboards the plate; DialogueWidgetComponent mirrors its widget.
	UPROPERTY(VisibleAnywhere, Category = "Rick|Dialogue")
	class UDialoguePlateComponent* DialoguePlate;

	// Player-only blocking capsule so the player can't walk into the NPC.
	UPROPERTY(VisibleAnywhere, Category = "Rick|Dialogue")
	class UNpcPersonalSpaceComponent* PersonalSpace;

	// Line index parsed from [Bladder] tag in dialogue file (-1 if absent)
	int32 BladderPulseLineIndex = INDEX_NONE;

protected:
	// Sectioned dialogue file (relative to Content/), parsed by FDialogueScript
	UPROPERTY(EditAnywhere, Category = "Rick|Dialogue")
	FString DialogueFilePath = TEXT("Dialogue/Rick.txt");

	UPROPERTY(EditAnywhere, Category = "Rick|Dialogue")
	FString CarRideSection = TEXT("CarRide");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rick|Money")
	UItemDefinition* MoneyDef;

	// Line index in GivesMoneyLines at which money is added to inventory (0-based)
	UPROPERTY(EditAnywhere, Category = "Rick|Money")
	int32 MoneyGiveLineIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rick|Outside")
	ASeneca* SenecaRef;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rick|Outside")
	AActor* OutsidePositionTarget;

	// The car actor placed in the level — teleported alongside Rick after the ride
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rick|Outside")
	AActor* CarActor;

	// Offset applied to OutsidePositionTarget when placing the car (so it doesn't overlap Rick)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rick|Outside")
	FVector CarActorOffset = FVector(0.f, 250.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Rick|Outside")
	FString OutsideIdleSection = TEXT("RickOutsideIdle");

	UPROPERTY(EditAnywhere, Category = "Rick|Outside")
	FString GivesMoneySection = TEXT("RickGivesMoney");

	// Where Rick stands once he's come into the store (assign on the level instance).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rick|Inside")
	AActor* InsidePositionTarget;

	// Inside, Rick translates whichever Czech request Seneca has open. Hearing
	// the intro or blank-tape translation lets Seneca take the movies / tape.
	UPROPERTY(EditAnywhere, Category = "Rick|Inside")
	FString IntroTranslationSection = TEXT("RickTranslateIntro");

	UPROPERTY(EditAnywhere, Category = "Rick|Inside")
	FString BlankTapeTranslationSection = TEXT("RickTranslateBlankTape");

	UPROPERTY(EditAnywhere, Category = "Rick|Inside")
	FString KeyTranslationSection = TEXT("RickTranslateKey");

	// Inside idle line whenever there's nothing to translate.
	UPROPERTY(EditAnywhere, Category = "Rick|Inside")
	FString InsideIdleSection = TEXT("RickInsideIdle");

private:
	// Parsed sectioned dialogue file (DialogueFilePath)
	FDialogueScript DialogueScript;

	// Parsed car ride dialogue lines (speaker + text per line)
	TArray<FSimpleDialogueLine> ParsedLines;

	TArray<FText> OutsideIdleLines;
	TArray<FSimpleDialogueLine> GivesMoneyLines;
	TArray<FText> IntroTranslationLines;
	TArray<FText> BlankTapeTranslationLines;
	TArray<FText> KeyTranslationLines;
	TArray<FText> InsideIdleLines;

	bool bGaveMoney = false;
	bool bMoneyBeatArmed = false;

	// The player walked back into the store after Seneca's intro -> Rick goes
	// inside as soon as neither spot is in view (Tick). bInside flips when the
	// move has happened.
	bool bPendingTeleportInside = false;
	bool bInside = false;

	// A translation is the dialogue playing (OnDialogueEnded tells Seneca).
	bool bTranslationActive = false;

	// Lines translating Seneca's open request, or null when he has none.
	const TArray<FText>* GetSenecaTranslation() const;

	// UStorySubsystem::OnPlayerEnteredStore handler.
	void OnPlayerEnteredStore();

	// Storm stash state (StashForStorm / ReturnFromStorm).
	bool bStashedForStorm = false;
	FTransform PreStormTransform;

	void LoadDialogueFile();
	void LoadOutsideDialogue();

	UFUNCTION()
	void OnMoneyDialogueLineShown(int32 LineIndex);
};
