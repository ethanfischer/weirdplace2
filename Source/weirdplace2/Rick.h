#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rick.generated.h"

class UWidgetComponent;
struct FSimpleDialogueLine;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRickDialogueEnded);

UCLASS()
class WEIRDPLACE2_API ARick : public AActor
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

private:
	// Parsed dialogue lines (speaker + text per line)
	TArray<FSimpleDialogueLine> ParsedLines;

	// Load and parse the dialogue file
	void LoadDialogueFile();
};
