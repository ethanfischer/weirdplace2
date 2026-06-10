#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GazeRewardActor.generated.h"

class UAudioComponent;
class UItemDefinition;
class USoundBase;

// Stare at the GazeTarget actor long enough and this pays out. Gaze counts
// when the camera's center ray actually hits the target — any point on it,
// which matters for long fixtures like the canopy bar light. A hum swells as
// the held gaze approaches RequiredLookSeconds; looking away resets both the
// timer and the hum. Grants once per game.
UCLASS()
class WEIRDPLACE2_API AGazeRewardActor : public AActor
{
	GENERATED_BODY()

public:
	AGazeRewardActor();

	virtual void Tick(float DeltaTime) override;

	// The actor the player must stare at (set by UGazeRewardSubsystem)
	UPROPERTY(EditAnywhere, Category = "Gaze Reward")
	AActor* GazeTarget = nullptr;

	// Item granted when the stare completes
	UPROPERTY(EditAnywhere, Category = "Gaze Reward")
	UItemDefinition* RewardItem = nullptr;

	// Continuous look time required, in seconds
	UPROPERTY(EditAnywhere, Category = "Gaze Reward")
	float RequiredLookSeconds = 30.f;

	// Sound that swells while the gaze is held
	UPROPERTY(EditAnywhere, Category = "Gaze Reward")
	USoundBase* HumSound = nullptr;

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
