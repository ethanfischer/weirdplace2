#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FootstepComponent.generated.h"

class USoundBase;

// Plays a randomized footstep sound each stride while the owning character is
// walking on the ground. Diegetic: sounds are played at the character's feet.
// Sound assets are loaded from /Game/Sounds/Footsteps at BeginPlay — drop new
// variants in that folder and they're picked up automatically.
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFootstepComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void PlayFootstep();
	void LoadSet(int32 SetIndex);

	// Subfolders of /Game/Sounds/Footsteps, sorted; weird.Footstep.Set indexes this.
	TArray<FString> SetPaths;
	int32 LoadedSetIndex = INDEX_NONE;

	// Sounds of the currently selected set.
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> FootstepSounds;

	// Seconds since the last footstep (steps fire on a fixed interval).
	float TimeSinceLastStep = 0.f;
	bool bWasWalking = false;

	// Index into FootstepSounds of the last variant played, to avoid repeats.
	int32 LastSoundIndex = INDEX_NONE;
};
