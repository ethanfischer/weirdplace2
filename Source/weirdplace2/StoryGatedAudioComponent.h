#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "StorySubsystem.h"
#include "StoryGatedAudioComponent.generated.h"

// An AudioComponent that stays silent until a story beat is reached, then
// fades in and keeps playing. Add it to any actor (e.g. the Oasis piano) and
// pick the gating flag; spatialization/occlusion come from the usual
// AttenuationSettings on the component. Never auto-activates.
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class WEIRDPLACE2_API UStoryGatedAudioComponent : public UAudioComponent
{
	GENERATED_BODY()

public:
	UStoryGatedAudioComponent();

	// Beat that starts playback. Already set at BeginPlay also starts it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story Gate")
	EStoryFlag GateFlag = EStoryFlag::EnteredOasis;

	// Fade-in length once the beat fires (seconds).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story Gate", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story Gate")
	EAudioFaderCurve FadeInCurve = EAudioFaderCurve::Logarithmic;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnStoryFlagChanged(EStoryFlag Flag, bool bValue);
	void StartGatedPlayback();

	FDelegateHandle FlagChangedHandle;
};
