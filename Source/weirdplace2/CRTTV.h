#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "CRTTV.generated.h"

class UMediaPlayer;
class UMediaSource;
class UMediaSoundComponent;
class UMaterialInterface;
class UTexture2D;
class USoundBase;
class UAudioComponent;

UCLASS()
class WEIRDPLACE2_API ACRTTV : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	ACRTTV();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TV")
	UMediaPlayer* MediaPlayer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TV")
	UMediaSource* MediaSource = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TV")
	UMediaSoundComponent* MediaSound = nullptr;

	// Placeholder emergency-broadcast screen (default-loads M_TornadoWarning if
	// unset). The material exposes a "ScreenTex" texture param; WarningScreenTexture
	// is fed into it via a MID so the designer's art shows over the red fallback.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TV")
	UMaterialInterface* WarningScreenMaterial = nullptr;

	// Designer drops their "TORNADO WARNING" image here (set on BP_TV defaults so
	// both TVs share one assignment). Until assigned, the material's red fallback
	// texture shows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TV")
	UTexture2D* WarningScreenTexture = nullptr;

	// Tornado-alert siren played diegetically from the TV (default-loads
	// /Game/Sounds/tornadoalert if unset). The wave is a one-shot; the looping
	// (with a gap) is driven in C++ — see WarningLoopGapSeconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TV")
	USoundBase* WarningSound = nullptr;

	// Silent gap between siren loops, in seconds (set on BP_TV defaults). The clip
	// plays through once, waits this long, then repeats — so it doesn't loop
	// instantly. 0 = replay immediately (still a one-frame timer hop, not seamless).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"), Category = "TV")
	float WarningLoopGapSeconds = 1.5f;

	// Material slot index of the CRT screen — the warning material is applied here.
	// Set on BP_TV defaults; all CRT TVs share the mesh, so one value covers every
	// instance. Out of range = no warning screen (logged, not inferred).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TV")
	int32 ScreenMaterialSlot = INDEX_NONE;

	// Stop the media feed, swap the screen slot to the warning material (with the
	// designer texture if set), and blare the looping alarm. Idempotent. Drives
	// item 1 (store-entry-after-key-break tornado warning).
	void ShowTornadoWarning();

	bool IsShowingWarning() const { return bShowingWarning; }

	// Silence the looping siren for good while leaving the warning screen up.
	// Driven by the StorySubsystem once the player uses the payphone. Idempotent.
	void StopWarningAudio();

	// True while the looping tornado-alert siren is playing (test query).
	bool IsWarningAudioPlaying() const;

protected:
	virtual void BeginPlay() override;

private:
	// Spatialized siren built at runtime in BeginPlay, attached to the screen mesh.
	UPROPERTY()
	UAudioComponent* WarningAudio = nullptr;

	// Re-fires the siren after WarningLoopGapSeconds once the one-shot finishes.
	UFUNCTION()
	void OnWarningAudioFinished();

	// Start one play of the siren.
	void PlayWarningLoop();

	FTimerHandle WarningLoopTimer;
	bool bShowingWarning = false;
	// Set by StopWarningAudio; blocks any further siren replays.
	bool bWarningAudioStopped = false;
};
