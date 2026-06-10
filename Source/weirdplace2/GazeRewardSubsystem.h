#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GazeRewardSubsystem.generated.h"

// Spawns the gas-station gaze-reward actor at runtime. Editor-time placement
// via Python crashes UE 5.7's headless actor factories, so the level only
// carries a 'GazeRewardTarget' tag on the stared-at light; this subsystem
// finds the tagged actor at world start and spawns AGazeRewardActor at its
// bounds center. Configured in DefaultGame.ini under
// [/Script/weirdplace2.GazeRewardSubsystem].
UCLASS(Config=Game)
class WEIRDPLACE2_API UGazeRewardSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	UPROPERTY(Config)
	FSoftObjectPath RewardItemPath;

	UPROPERTY(Config)
	FSoftObjectPath HumSoundPath;

	UPROPERTY(Config)
	float RequiredLookSeconds = 30.f;
};
