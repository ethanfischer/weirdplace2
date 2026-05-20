#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "Components/AudioComponent.h"
class ATargetPoint;
class AAmbientSound;
class ADoor;
class UBladderUrgencyComponent;
#include "TeleportTriggerBox.generated.h"

UCLASS()
class WEIRDPLACE2_API ATeleportTriggerBox : public ATriggerBox
{
	GENERATED_BODY()

public:
	ATeleportTriggerBox();

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

protected:
	// Destroys Ultra Dynamic Sky actors if present (avoids hard dependency)
	void DestroyUltraDynamicActors();

	// Fades out the AmbientGlobalWind AAmbientSound if assigned
	void SilenceGlobalWindIfRequested();

	// Fades in the given AAmbientSound over Duration with FadeCurve
	void FadeInAmbient(AAmbientSound* Ambient, float Duration, EAudioFaderCurve FadeCurve = EAudioFaderCurve::Linear);

	UFUNCTION()
	void UnlockBathroomStallDoor();

	// --- Properties ---

	// Required target point actor to teleport to (set on the level instance)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport")
	ATargetPoint* TeleportTarget;

	// Whether to destroy Ultra Dynamic Sky actors on teleport
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
	bool bDestroyUltraDynamicActors = false;

	// Whether to fade out Ambient_GlobalWind (and fade in Ambient_Waterfall) on teleport
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport")
	bool bSilenceGlobalWind = false;

	// Whether to stop the player's bladder urgency (pulses + death timer + vignette) on teleport
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport")
	bool bStopBladderUrgency = false;

	// Ambient sound to fade out when bSilenceGlobalWind is set (set on level instance)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport", meta = (EditCondition = "bSilenceGlobalWind"))
	AAmbientSound* AmbientGlobalWind = nullptr;

	// Ambient sound to fade in when bSilenceGlobalWind is set (set on level instance)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport", meta = (EditCondition = "bSilenceGlobalWind"))
	AAmbientSound* AmbientWaterfall = nullptr;

	// Ambient sound to fade in when bSilenceGlobalWind is set (set on level instance)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport", meta = (EditCondition = "bSilenceGlobalWind"))
	AAmbientSound* AmbientChord = nullptr;

	// Fade-out duration for AmbientGlobalWind (only used when bSilenceGlobalWind is true)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport", meta = (EditCondition = "bSilenceGlobalWind"))
	float WindFadeOutDuration = 10.0f;

	// Fade-in duration for AmbientWaterfall (only used when bSilenceGlobalWind is true)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport", meta = (EditCondition = "bSilenceGlobalWind"))
	float WaterfallFadeInDuration = 10.0f;

	// Fade-in curve for Ambient_Waterfall
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport", meta = (EditCondition = "bSilenceGlobalWind"))
	EAudioFaderCurve WaterfallFadeCurve = EAudioFaderCurve::Logarithmic;

	// Fade-in duration for Ambient_Chord (only used when bSilenceGlobalWind is true)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport", meta = (EditCondition = "bSilenceGlobalWind"))
	float ChordFadeInDuration = 10.0f;

	// Fade-in curve for Ambient_Chord
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport", meta = (EditCondition = "bSilenceGlobalWind"))
	EAudioFaderCurve ChordFadeCurve = EAudioFaderCurve::Logarithmic;

	// Door to unlock after BathroomStallDoorUnlockTime seconds (set on level instance)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport")
	ADoor* BathroomStallDoor = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport", meta = (EditCondition = "BathroomStallDoor != nullptr"))
	float BathroomStallDoorUnlockTime = 60.0f;

	FTimerHandle BathroomStallDoorUnlockTimerHandle;
};
