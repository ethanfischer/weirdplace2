#include "CRTTV.h"

#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaSoundComponent.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace CRTTVConst
{
	static const TCHAR* WarningSoundPath = TEXT("/Game/Sounds/tornadoalert.tornadoalert");
	static const FName ScreenTexParam(TEXT("ScreenTex"));
	static const FName UseScreenTexParam(TEXT("UseScreenTex"));
}

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

	// Build the siren audio at runtime (an actor component owned by this actor),
	// attached to the screen mesh and spatialized so it's loudest right at the TV.
	// Silent until ShowTornadoWarning plays it. Mirrors UGazeRewardComponent's
	// runtime-audio pattern.
	WarningAudio = NewObject<UAudioComponent>(this, TEXT("WarningAudio"));
	WarningAudio->SetupAttachment(GetStaticMeshComponent());
	WarningAudio->bAutoActivate = false;
	WarningAudio->bAllowSpatialization = true;
	// Override attenuation so the siren is a real point source (falls off with
	// distance) instead of blasting at full volume across the whole level — it
	// should be loudest right at the TVs and fade as the player walks away.
	WarningAudio->bOverrideAttenuation = true;
	WarningAudio->AttenuationOverrides.bAttenuate = true;
	WarningAudio->AttenuationOverrides.bSpatialize = true;
	WarningAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(200.f, 0.f, 0.f);
	WarningAudio->AttenuationOverrides.FalloffDistance = 2000.f;
	// Occlusion so the store walls block the siren — without it the sound bleeds
	// straight through to the parking lot. A Visibility line-trace from listener to
	// source; when blocked, drop the volume hard and low-pass it so only a muffled
	// hint leaks (and at the open doorway you still hear it, which is realistic).
	WarningAudio->AttenuationOverrides.bEnableOcclusion = true;
	WarningAudio->AttenuationOverrides.OcclusionTraceChannel = ECC_Visibility;
	WarningAudio->AttenuationOverrides.OcclusionLowPassFilterFrequency = 500.f;
	WarningAudio->AttenuationOverrides.OcclusionVolumeAttenuation = 0.1f;
	WarningAudio->AttenuationOverrides.OcclusionInterpolationTime = 0.2f;
	WarningAudio->RegisterComponent();

	// The wave is a one-shot; re-fire it after the gap each time it finishes so
	// the siren loops with a designer-set silence between repeats.
	WarningAudio->OnAudioFinished.AddDynamic(this, &ACRTTV::OnWarningAudioFinished);
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

	// Drive the screen through a MID so the designer's "TORNADO WARNING" texture
	// can be fed into the material's ScreenTex param. The material lerps between a
	// storm-red fallback and ScreenTex on the UseScreenTex switch, so the screen
	// stays red until WarningScreenTexture is assigned, then shows the art untinted.
	UMaterialInstanceDynamic* ScreenMID = UMaterialInstanceDynamic::Create(WarningScreenMaterial, this);
	if (ScreenMID && WarningScreenTexture)
	{
		ScreenMID->SetTextureParameterValue(CRTTVConst::ScreenTexParam, WarningScreenTexture);
		ScreenMID->SetScalarParameterValue(CRTTVConst::UseScreenTexParam, 1.f);
	}
	Mesh->SetMaterial(ScreenSlot, ScreenMID ? static_cast<UMaterialInterface*>(ScreenMID) : WarningScreenMaterial);

	// Blare the looping tornado-alert siren from the TV. MediaPlayer->Close()
	// above already silenced the TV's own media audio.
	if (!WarningSound)
	{
		WarningSound = LoadObject<USoundBase>(nullptr, CRTTVConst::WarningSoundPath);
	}
	if (WarningAudio && WarningSound)
	{
		WarningAudio->SetSound(WarningSound);
		PlayWarningLoop();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ACRTTV %s: ShowTornadoWarning - missing WarningAudio(%d)/WarningSound(%d), no siren"),
			*GetName(), WarningAudio ? 1 : 0, WarningSound ? 1 : 0);
	}

	bShowingWarning = true;
	UE_LOG(LogTemp, Log, TEXT("ACRTTV %s: tornado warning shown (screen slot %d), siren playing=%d, loop gap=%.2fs"),
		*GetName(), ScreenSlot, IsWarningAudioPlaying() ? 1 : 0, WarningLoopGapSeconds);
}

void ACRTTV::PlayWarningLoop()
{
	if (WarningAudio)
	{
		WarningAudio->Play();
	}
}

void ACRTTV::OnWarningAudioFinished()
{
	// Keep the siren going for the duration of the warning, with a silent gap so
	// it doesn't loop instantly.
	if (!bShowingWarning)
	{
		return;
	}
	if (WarningLoopGapSeconds <= 0.f)
	{
		PlayWarningLoop();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(WarningLoopTimer, this, &ACRTTV::PlayWarningLoop,
			WarningLoopGapSeconds, /*bLoop*/ false);
	}
}

bool ACRTTV::IsWarningAudioPlaying() const
{
	if (WarningAudio && WarningAudio->IsPlaying())
	{
		return true;
	}
	// During the inter-loop gap the component is silent, but the siren loop is
	// still running — a replay timer is pending.
	UWorld* World = GetWorld();
	return bShowingWarning && World && World->GetTimerManager().IsTimerActive(WarningLoopTimer);
}
