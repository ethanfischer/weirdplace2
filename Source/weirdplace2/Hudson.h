#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DialogueWidgetProvider.h"
#include "DialogueScript.h"
#include "Hudson.generated.h"

class UWidgetComponent;
class UUI_Dialogue;

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
	virtual UUI_Dialogue* GetDialogueWidget() const override;

	void OnDialogueEnded();

	// Inventory give-mode callback: accepts Money when offered.
	bool OnMoneyOffered(FName ItemID);

	UPROPERTY(BlueprintReadOnly, Category = "Hudson")
	UWidgetComponent* DialogueWidgetComponent;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Hudson")
	EHudsonState CurrentState = EHudsonState::Idle;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Sectioned dialogue file (relative to Content/), parsed by FDialogueScript
	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString DialogueFilePath = TEXT("Dialogue/Hudson.txt");

	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString IdleSection = TEXT("HudsonIdle");

	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString BegSection = TEXT("HudsonBeg");

	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString ThankYouSection = TEXT("HudsonThankYou");

private:
	TArray<FText> IdleLines;
	TArray<FText> BegLines;
	TArray<FText> ThankYouLines;

	// bLastDialogueWasBeg tracks whether the most recent dialogue started from Idle+Money state
	bool bLastDialogueWasBeg = false;

	// Parsed sectioned dialogue file (DialogueFilePath)
	FDialogueScript DialogueScript;

	void LoadDialogue(const FString& SectionName, TArray<FText>& OutLines);
};
