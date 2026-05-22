#include "CRTTV.h"

#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaSoundComponent.h"

ACRTTV::ACRTTV()
{
	PrimaryActorTick.bCanEverTick = false;
	GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);

	MediaSound = CreateDefaultSubobject<UMediaSoundComponent>(TEXT("MediaSound"));
	MediaSound->SetupAttachment(GetStaticMeshComponent());
	MediaSound->bAutoActivate = true;
}

void ACRTTV::BeginPlay()
{
	Super::BeginPlay();

	if (!MediaPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("ACRTTV %s: MediaPlayer is not set"), *GetName());
		return;
	}
	if (!MediaSource)
	{
		UE_LOG(LogTemp, Error, TEXT("ACRTTV %s: MediaSource is not set"), *GetName());
		return;
	}

	const bool bOpened = MediaPlayer->OpenSource(MediaSource);
	UE_LOG(LogTemp, Log, TEXT("ACRTTV %s: OpenSource(%s) -> %s"),
		*GetName(), *MediaSource->GetName(), bOpened ? TEXT("OK") : TEXT("FAILED"));

	MediaSound->SetMediaPlayer(MediaPlayer);
}
