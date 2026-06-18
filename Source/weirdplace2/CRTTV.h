#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "CRTTV.generated.h"

class UMediaPlayer;
class UMediaSource;
class UMediaSoundComponent;
class UMaterialInterface;

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
	// unset). Real warning feed swapped in later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TV")
	UMaterialInterface* WarningScreenMaterial = nullptr;

	// Stop the media feed and swap the screen slot to the warning material.
	// Idempotent. Drives item 1 (store-entry-after-key-break tornado warning).
	void ShowTornadoWarning();

	bool IsShowingWarning() const { return bShowingWarning; }

protected:
	virtual void BeginPlay() override;

private:
	bool bShowingWarning = false;
};
