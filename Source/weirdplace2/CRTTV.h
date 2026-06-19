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

	// Tornado-alert siren looped diegetically from the TV (default-loads
	// /Game/Sounds/tornadoalert if unset).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TV")
	USoundBase* WarningSound = nullptr;

	// Stop the media feed, swap the screen slot to the warning material (with the
	// designer texture if set), and blare the looping alarm. Idempotent. Drives
	// item 1 (store-entry-after-key-break tornado warning).
	void ShowTornadoWarning();

	bool IsShowingWarning() const { return bShowingWarning; }

	// True while the looping tornado-alert siren is playing (test query).
	bool IsWarningAudioPlaying() const;

protected:
	virtual void BeginPlay() override;

private:
	// Spatialized siren built at runtime in BeginPlay, attached to the screen mesh.
	UPROPERTY()
	UAudioComponent* WarningAudio = nullptr;

	bool bShowingWarning = false;
};
