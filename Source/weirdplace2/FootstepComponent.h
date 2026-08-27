#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FootstepComponent.generated.h"

class USoundBase;

// Plays a randomized footstep sound at a fixed interval while the owning
// character walks. Diegetic: sounds are played at the character's feet.
//
// Sound sets are the subfolders of /Game/Sounds/Footsteps (e.g. Carpet,
// WetDirt), all loaded at BeginPlay. Each step line-traces to the floor and
// picks the set named by a "Footstep.<SetName>" actor tag on whatever it hit
// (same tag pattern as Storm*); untagged floors fall back to the
// weird.Footstep.Set cvar index.
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFootstepComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FFootstepSet
	{
		FString Name;
		TArray<USoundBase*> Sounds;
	};

	void PlayFootstep();
	// Resolve which set index to use for this step: a Footstep.<SetName>-tagged
	// volume containing the feet wins, then the floor trace's tag, then the
	// weird.Footstep.Set fallback.
	int32 ResolveSetIndex() const;
	// Index of the set named by this actor's Footstep.<SetName> tag, or INDEX_NONE.
	int32 SetIndexFromTags(const AActor* Actor) const;

	// All sets, sorted by name. Sounds are rooted via KeepAliveSounds.
	TArray<FFootstepSet> Sets;

	// GC root for every loaded footstep sound (FFootstepSet isn't a USTRUCT).
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> KeepAliveSounds;

	// Seconds since the last footstep (steps fire on a fixed interval).
	float TimeSinceLastStep = 0.f;
	bool bWasWalking = false;

	// Last variant played per set, to avoid immediate repeats.
	TArray<int32> LastSoundIndexPerSet;

	// Footstep.<SetName>-tagged AVolumes in the level (e.g. the AV_* audio
	// volumes), collected at BeginPlay. Checked before the floor trace so a
	// region within one floor mesh can override its surface.
	UPROPERTY()
	TArray<TObjectPtr<AActor>> TaggedVolumes;

	// Floor actor hit by the last step's trace (diagnostics for tag setup).
	mutable TWeakObjectPtr<AActor> LastFloorActor;
};
