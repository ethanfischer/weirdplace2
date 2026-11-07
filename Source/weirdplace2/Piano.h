#pragma once

#include "CoreMinimal.h"
#include "FlavorProp.h"
#include "Typewriter.h"
#include "Piano.generated.h"

class UDiegeticTextComponent;
class UAudioComponent;

// The oasis piano (BP_Piano is reparented onto this). Its music is a
// UStoryGatedAudioComponent on the Blueprint. Interacting with it starts the
// end of the demo: weird.Demo.EndNoticeDelay seconds later, an "end of the
// demo" notice types itself out in the world in front of the player.
UCLASS()
class WEIRDPLACE2_API APiano : public AFlavorProp
{
	GENERATED_BODY()

public:
	APiano();

	// What the end-of-demo notice shows right now ("" until it appears; tests).
	FString GetEndNoticeDisplayText() const;
	bool IsEndNoticeArmed() const { return bEndNoticeArmed; }

	// Start the end-of-demo countdown (first call only). Interacting with the
	// piano, or talking to Seneca at it, calls this.
	void ArmEndNotice();

	// Pause / resume the piano music where it is (the pianist stops to talk,
	// then plays on from the same spot).
	void SetMusicPaused(bool bPaused);
	bool IsMusicPaused() const;
	bool IsMusicPlaying() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piano|End Of Demo")
	FString EndNoticeText = TEXT("You have reached the end of the demo.\nThanks for playing.");

	// How far in front of the player's eyes the notice appears (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piano|End Of Demo")
	float EndNoticeDistance = 300.f;

	// Where the pianist (Seneca, once the player reaches the oasis) stands, as
	// an actor transform for BP_Seneca (whose body faces actor yaw + 90).
	// Default: beside the stool at the keyboard's end, turned toward the
	// player's approach. Drag it in BP_Piano to adjust.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Piano")
	USceneComponent* PianistSpot = nullptr;

protected:
	virtual void OnFlavorInteract() override;

private:
	void ShowEndNotice();

	// BP_Piano's PianoMusic (UStoryGatedAudioComponent); null + error if missing.
	UAudioComponent* GetMusic() const;

	UPROPERTY()
	UDiegeticTextComponent* EndNotice = nullptr;

	FTypewriterReveal EndNoticeTypewriter;
	FTimerHandle EndNoticeTimer;
	bool bEndNoticeArmed = false;
};
