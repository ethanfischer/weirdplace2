#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GazeRewardActor.generated.h"

class UAudioComponent;
class UItemDefinition;
class USoundBase;

// Stare at this actor's location long enough and it pays out. Place it at the
// bounds center of the thing being stared at (e.g. a gas-station canopy
// light). A hum swells as the held gaze approaches RequiredLookSeconds;
// looking away resets both the timer and the hum. Grants once per game.
UCLASS()
class WEIRDPLACE2_API AGazeRewardActor : public AActor
{
	GENERATED_BODY()

public:
	AGazeRewardActor();

	virtual void Tick(float DeltaTime) override;

	// Item granted when the stare completes
	UPROPERTY(EditAnywhere, Category = "Gaze Reward")
	UItemDefinition* RewardItem = nullptr;

	// Continuous look time required, in seconds
	UPROPERTY(EditAnywhere, Category = "Gaze Reward")
	float RequiredLookSeconds = 30.f;

	// Sound that swells while the gaze is held
	UPROPERTY(EditAnywhere, Category = "Gaze Reward")
	USoundBase* HumSound = nullptr;

	// Half-angle of the cone within which the camera counts as "looking", degrees
	UPROPERTY(EditAnywhere, Category = "Gaze Reward")
	float GazeConeHalfAngleDegrees = 10.f;

	// Beyond this camera distance the gaze never counts, in cm
	UPROPERTY(EditAnywhere, Category = "Gaze Reward")
	float MaxGazeDistance = 6000.f;

private:
	bool IsPlayerGazingAtMe() const;
	void GrantReward();

	UPROPERTY(VisibleAnywhere, Category = "Gaze Reward")
	UAudioComponent* HumComponent;

	float GazeSeconds = 0.f;
	bool bRewardGranted = false;
};
