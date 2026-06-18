#include "CRTTV.h"

#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaSoundComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

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

void ACRTTV::ShowTornadoWarning()
{
	if (bShowingWarning)
	{
		return;
	}

	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}

	if (!WarningScreenMaterial)
	{
		WarningScreenMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CreatedMaterials/M_TornadoWarning.M_TornadoWarning"));
	}
	if (!WarningScreenMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("ACRTTV %s: ShowTornadoWarning - M_TornadoWarning material not found"), *GetName());
		return;
	}

	UStaticMeshComponent* Mesh = GetStaticMeshComponent();
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ACRTTV %s: ShowTornadoWarning - no static mesh component"), *GetName());
		return;
	}

	// The screen is the material slot currently driven by the CRT screen material
	// (its name contains "Screen"); the other slots are the bezel/back casing.
	const TArray<UMaterialInterface*> Mats = Mesh->GetMaterials();
	int32 ScreenSlot = INDEX_NONE;
	for (int32 i = 0; i < Mats.Num(); ++i)
	{
		if (Mats[i] && Mats[i]->GetName().Contains(TEXT("Screen")))
		{
			ScreenSlot = i;
			break;
		}
	}
	if (ScreenSlot == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("ACRTTV %s: ShowTornadoWarning - no screen material slot found among %d slots"), *GetName(), Mats.Num());
		return;
	}

	Mesh->SetMaterial(ScreenSlot, WarningScreenMaterial);
	bShowingWarning = true;
	UE_LOG(LogTemp, Log, TEXT("ACRTTV %s: tornado warning shown (screen slot %d)"), *GetName(), ScreenSlot);
}
