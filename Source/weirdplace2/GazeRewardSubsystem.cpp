#include "GazeRewardSubsystem.h"
#include "EngineUtils.h"
#include "GazeRewardActor.h"
#include "ItemDefinition.h"
#include "Sound/SoundBase.h"

void UGazeRewardSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld())
	{
		return;
	}

	AActor* Target = nullptr;
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		if (It->ActorHasTag(FName("GazeRewardTarget")))
		{
			Target = *It;
			break;
		}
	}
	if (!Target)
	{
		// Worlds without a rigged light (entry map, test worlds) have nothing to do.
		UE_LOG(LogTemp, Verbose, TEXT("GazeRewardSubsystem: no 'GazeRewardTarget' actor in %s"), *InWorld.GetName());
		return;
	}

	UItemDefinition* RewardItem = Cast<UItemDefinition>(RewardItemPath.TryLoad());
	if (!RewardItem)
	{
		UE_LOG(LogTemp, Error, TEXT("GazeRewardSubsystem: RewardItemPath '%s' failed to load"), *RewardItemPath.ToString());
		return;
	}

	USoundBase* Hum = Cast<USoundBase>(HumSoundPath.TryLoad());
	if (!Hum)
	{
		UE_LOG(LogTemp, Error, TEXT("GazeRewardSubsystem: HumSoundPath '%s' failed to load"), *HumSoundPath.ToString());
		return;
	}

	const FVector Center = Target->GetComponentsBoundingBox(/*bNonColliding*/ true).GetCenter();
	AGazeRewardActor* Gaze = InWorld.SpawnActor<AGazeRewardActor>(Center, FRotator::ZeroRotator);
	if (!Gaze)
	{
		UE_LOG(LogTemp, Error, TEXT("GazeRewardSubsystem: failed to spawn AGazeRewardActor"));
		return;
	}

	Gaze->GazeTarget = Target;
	Gaze->RewardItem = RewardItem;
	Gaze->HumSound = Hum;
	Gaze->RequiredLookSeconds = RequiredLookSeconds;
	UE_LOG(LogTemp, Log, TEXT("GazeRewardSubsystem: gaze reward live at %s (target '%s')"),
		*Center.ToString(), *Target->GetName());
}
