#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FirstPersonCharacter.h"
#include "Interactable.h"
#include "DialogueWidgetProvider.h"
#include "Rick.generated.h"

class UWidgetComponent;
class UStaticMesh;
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

public:
	// Start the car ride dialogue on the player character
	void StartDialogue();

	// Called by FirstPersonCharacter when multi-speaker dialogue ends
	void OnDialogueEnded();

	// IInteractable
	virtual void Interact_Implementation() override;

	// IDialogueWidgetProvider
	virtual UUI_Dialogue* GetDialogueWidget() const override;

	// Reveal Rick at his outside-store position (called by CarRideComponent after fade)
	void AppearOutside();

	// Delegate fired when dialogue ends (CarRideComponent binds to this)
	UPROPERTY(BlueprintAssignable, Category = "Rick|Dialogue")
	FOnRickDialogueEnded OnRickDialogueEnded;

	// Widget component hosting the dialogue UI - auto-found by name in BeginPlay
	UPROPERTY(BlueprintReadOnly, Category = "Rick|Dialogue")
	UWidgetComponent* DialogueWidgetComponent;

protected:
	// Path to dialogue text file (relative to Content/)
	UPROPERTY(EditAnywhere, Category = "Rick|Dialogue")
	FString CarDialoguePath = TEXT("Dialogue/CarRide.txt");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rick|Money")
	UStaticMesh* MoneyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rick|Money")
	FVector MoneyScale = FVector(1.f, 1.f, 1.f);

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
	FString RickOutsideIdlePath = TEXT("Dialogue/RickOutsideIdle.txt");

	UPROPERTY(EditAnywhere, Category = "Rick|Outside")
	FString RickGivesMoneyPath = TEXT("Dialogue/RickGivesMoney.txt");

private:
	// Parsed car ride dialogue lines (speaker + text per line)
	TArray<FSimpleDialogueLine> ParsedLines;

	TArray<FText> OutsideIdleLines;
	TArray<FSimpleDialogueLine> GivesMoneyLines;

	bool bGaveMoney = false;

	void LoadDialogueFile();
	void LoadOutsideDialogue();

	UFUNCTION()
	void OnMoneyDialogueLineShown(int32 LineIndex);
};
