#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "CRTTV.generated.h"

class UMediaPlayer;
class UMediaSource;
class UMediaSoundComponent;

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

protected:
	virtual void BeginPlay() override;
};
