#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Styling/SlateColor.h"
#include "CarRideComponent.generated.h"

class ARick;
class UBladderUrgencyComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEIRDPLACE2_API UCarRideComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCarRideComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Level instance references ---

	// Root actor containing all moving scenery as children
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	AActor* SceneryRoot;

	// The driver NPC
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	ARick* Rick;

	// Empty actor at passenger seat position (player teleports here)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	AActor* PassengerSeatTarget;

	// Empty actor at store entrance (player teleports here when ride ends)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	AActor* ArrivalTarget;

	// --- Settings ---

	// Direction scenery moves (default: -X, simulating forward car travel)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	FVector SceneryMoveDirection = FVector(-1.0f, 0.0f, 0.0f);

	// Scenery speed in cm/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float ScenerySpeed = 1000.0f;

	// Seconds before dialogue begins
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float DialogueStartDelay = 3.0f;

	// Seconds of riding after dialogue ends
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float PostDialogueRideTime = 3.0f;

	// Fade to/from black duration in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float FadeDuration = 1.0f;

	// Empty actor positioned behind windshield where dialogue widget appears
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	AActor* DialogueWidgetTarget;

	// Text color for car ride dialogue (darker for readability against windshield)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	FSlateColor DialogueTextColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f));

	// Dialogue line index that triggers a single bladder pulse (0-based)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	int32 BladderPulseLineIndex = 3;

private:
	void StartRide();
	void StartDialogue();
	void EndRide();
	void OnFadeOutComplete();

	UFUNCTION()
	void OnDialogueEnded();

	UFUNCTION()
	void OnDialogueLineShown(int32 LineIndex);

	void OnBladderPulseFinished();

	bool bSceneryMoving = false;
	bool bBladderPulseArmed = false;

	FTimerHandle DialogueStartTimerHandle;
	FTimerHandle PostDialogueTimerHandle;
	FTimerHandle FadeOutTimerHandle;
	FTimerHandle BladderPulseTimerHandle;
};
