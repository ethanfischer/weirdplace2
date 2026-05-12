#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
class ATargetPoint;
class AAmbientSound;
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

	// Fades out the Ambient_GlobalWind AAmbientSound
	void SilenceGlobalWindIfRequested();

	// Fades in the Ambient_Waterfall AAmbientSound (paired with wind silence)
	void FadeInWaterfall();

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

	// Fade-in duration for Ambient_Waterfall (only used when bSilenceGlobalWind is true)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport", meta = (EditCondition = "bSilenceGlobalWind"))
	float WaterfallFadeInDuration = 10.0f;
};
