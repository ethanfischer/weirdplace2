#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DialogueWidgetProvider.h"
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

	UPROPERTY(BlueprintReadOnly, Category = "Hudson")
	UWidgetComponent* DialogueWidgetComponent;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Hudson")
	EHudsonState CurrentState = EHudsonState::Idle;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString HudsonIdlePath = TEXT("Dialogue/HudsonIdle.txt");

	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString HudsonBegPath = TEXT("Dialogue/HudsonBeg.txt");

	UPROPERTY(EditAnywhere, Category = "Hudson|Dialogue")
	FString HudsonThankYouPath = TEXT("Dialogue/HudsonThankYou.txt");

private:
	TArray<FText> IdleLines;
	TArray<FText> BegLines;
	TArray<FText> ThankYouLines;

	// bLastDialogueWasBeg tracks whether the most recent dialogue started from Idle+Money state
	bool bLastDialogueWasBeg = false;

	void LoadDialogue(const FString& RelPath, TArray<FText>& OutLines);
};
