#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DialogueWidgetProvider.h"
#include "DialogueScript.h"
#include "Hudson.generated.h"

class UWidgetComponent;
class UUI_Dialogue;
class AFirstPersonCharacter;

UENUM(BlueprintType)
enum class EHudsonState : uint8
{
	Idle,
	AwaitingDecision,
	GaveMoney
};

UCLASS()
class WEIRDPLACE2_API AHudson : public AActor, public IInteractable, public IDialogueWidgetProvider
{
	GENERATED_BODY()

public:
	AHudson();

	virtual void Interact_Implementation() override;
	virtual bool CanInteract() override { return !bGaveReward; } // done with the player once he's paid him back
	virtual UUI_Dialogue* GetDialogueWidget() const override;

	void OnDialogueEnded();

	// Inventory give-mode callback: accepts Money when offered.
	bool OnMoneyOffered(FName ItemID);

	UPROPERTY(BlueprintReadOnly, Category = "Hudson")
	UWidgetComponent* DialogueWidgetComponent;

	// Finds, positions and billboards the plate; DialogueWidgetComponent mirrors its widget.
	UPROPERTY(VisibleAnywhere, Category = "Hudson")
	class UDialoguePlateComponent* DialoguePlate;

	// Player-only blocking capsule so the player can't walk into the NPC.
	UPROPERTY(VisibleAnywhere, Category = "Hudson")
	class UNpcPersonalSpaceComponent* PersonalSpace;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Hudson")
	EHudsonState CurrentState = EHudsonState::Idle;

protected:
	virtual void BeginPlay() override;

	// Sectioned dialogue file (relative to Content/), parsed by FDialogueScript
	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString DialogueFilePath = TEXT("Dialogue/Hudson.txt");

	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString IdleSection = TEXT("HudsonIdle");

	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString BegSection = TEXT("HudsonBeg");

	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString ThankYouSection = TEXT("HudsonThankYou");

	// What Hudson hands over in return for the money (the meat tenderizer),
	// on the thank-you line that carries a [cue] in the dialogue file. None =
	// he just says thanks.
	UPROPERTY(EditAnywhere, Category = "Hudson")
	class UItemDefinition* RewardDef = nullptr;

private:
	TArray<FText> IdleLines;
	TArray<FText> BegLines;
	TArray<FText> ThankYouLines;

	// bLastDialogueWasBeg tracks whether the most recent dialogue started from Idle+Money state
	bool bLastDialogueWasBeg = false;

	// Parsed sectioned dialogue file (DialogueFilePath)
	FDialogueScript DialogueScript;

	// Thank-you line carrying the [Give ...] cue; INDEX_NONE = give when the
	// dialogue ends instead.
	int32 RewardLineIndex = INDEX_NONE;
	bool bGaveReward = false;

	UFUNCTION()
	void OnThankYouLineShown(int32 LineIndex);
	void GiveReward(AFirstPersonCharacter* FPChar);

	// OutCueLine (optional): index of the line that carries a [cue] tag.
	void LoadDialogue(const FString& SectionName, TArray<FText>& OutLines, int32* OutCueLine = nullptr);
};
