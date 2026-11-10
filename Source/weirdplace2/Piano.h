#pragma once

#include "CoreMinimal.h"
#include "FlavorProp.h"
#include "Piano.generated.h"

class UAudioComponent;

// The oasis piano (BP_Piano is reparented onto this). Its music is a
// UStoryGatedAudioComponent on the Blueprint; Seneca stands at PianistSpot and
// the music pauses while he talks.
UCLASS()
class WEIRDPLACE2_API APiano : public AFlavorProp
{
	GENERATED_BODY()

public:
	APiano();

	// Pause / resume the piano music where it is (the pianist stops to talk,
	// then plays on from the same spot).
	void SetMusicPaused(bool bPaused);
	bool IsMusicPaused() const;
	bool IsMusicPlaying() const;

	// Where the pianist (Seneca, once the player reaches the oasis) stands, as
	// an actor transform for BP_Seneca (whose body faces actor yaw + 90).
	// Default: beside the stool at the keyboard's end, turned toward the
	// player's approach. Drag it in BP_Piano to adjust.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Piano")
	USceneComponent* PianistSpot = nullptr;

private:
	// BP_Piano's PianoMusic (UStoryGatedAudioComponent); null + error if missing.
	UAudioComponent* GetMusic() const;
};
