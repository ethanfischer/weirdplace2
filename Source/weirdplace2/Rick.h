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

	// IDialogueWidgetProvider
	virtual UUI_Dialogue* GetDialogueWidget() const override;

	// Reveal Rick at his outside-store position (called by CarRideComponent after fade)
	void AppearOutside();

	// Storm beat: remove Rick from the world until a much-later beat brings him
	// back. MetaHuman grooms break if root visibility is toggled, so this
	// teleports him far below the level and disables collision instead.
	void StashForStorm();

	// Restore Rick to where he stood before the storm stash. (No caller yet —
	// the "much later" return beat will use this.)
	void ReturnFromStorm();

	// Delegate fired when dialogue ends (CarRideComponent binds to this)
	UPROPERTY(BlueprintAssignable, Category = "Rick|Dialogue")
	FOnRickDialogueEnded OnRickDialogueEnded;

	// Widget component hosting the dialogue UI - auto-found by name in BeginPlay
	UPROPERTY(BlueprintReadOnly, Category = "Rick|Dialogue")
	UWidgetComponent* DialogueWidgetComponent;

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

private:
	// Parsed sectioned dialogue file (DialogueFilePath)
	FDialogueScript DialogueScript;

	// Parsed car ride dialogue lines (speaker + text per line)
	TArray<FSimpleDialogueLine> ParsedLines;

	TArray<FText> OutsideIdleLines;
	TArray<FSimpleDialogueLine> GivesMoneyLines;

	bool bGaveMoney = false;
	bool bMoneyBeatArmed = false;

	// Storm stash state (StashForStorm / ReturnFromStorm).
	bool bStashedForStorm = false;
	FTransform PreStormTransform;

	void LoadDialogueFile();
	void LoadOutsideDialogue();

	UFUNCTION()
	void OnMoneyDialogueLineShown(int32 LineIndex);
};
