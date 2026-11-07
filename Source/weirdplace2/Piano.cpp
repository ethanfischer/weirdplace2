#include "Piano.h"
#include "Components/SceneComponent.h"
#include "StoryGatedAudioComponent.h"
#include "DiegeticTextComponent.h"
#include "Tunable.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

WP_TUNABLE_FLOAT(GEndNoticeDelay, "weird.Demo.EndNoticeDelay", 60.f,
	"Seconds after the player first interacts with the oasis piano before the end-of-demo notice appears.");
WP_TUNABLE_FLOAT(GEndNoticeCharInterval, "weird.Demo.EndNoticeCharInterval", 0.06f,
	"Typewriter pace of the end-of-demo notice (seconds per character).");

APiano::APiano()
{
	PianistSpot = CreateDefaultSubobject<USceneComponent>(TEXT("PianistSpot"));
	PianistSpot->SetupAttachment(GetRootComponent());
	// Piano-local: the keyboard faces -X here (the level piano is turned 180°).
	PianistSpot->SetRelativeLocation(FVector(-52.f, 137.f, -65.f));
	PianistSpot->SetRelativeRotation(FRotator(0.f, 119.f, 0.f));
}

void APiano::OnFlavorInteract()
{
	ArmEndNotice();
}

void APiano::ArmEndNotice()
{
	if (bEndNoticeArmed)
	{
		return;
	}
	bEndNoticeArmed = true;
	GetWorldTimerManager().SetTimer(EndNoticeTimer, this, &APiano::ShowEndNotice, FMath::Max(0.01f, GEndNoticeDelay), false);
	UE_LOG(LogTemp, Log, TEXT("APiano %s: end-of-demo notice in %.0fs"), *GetName(), GEndNoticeDelay);
}

void APiano::ShowEndNotice()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager : nullptr;
	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("APiano %s: no player camera; end-of-demo notice not shown"), *GetName());
		return;
	}

	// Fixed in the world where the player is looking (level with their eyes),
	// turning to face them — not glued to the view.
	FVector Forward = Camera->GetCameraRotation().Vector();
	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal();
	const FVector Location = Camera->GetCameraLocation() + Forward * EndNoticeDistance;

	EndNotice = NewObject<UDiegeticTextComponent>(this, TEXT("EndNotice"));
	EndNotice->SetupAttachment(GetRootComponent());
	EndNotice->RegisterComponent();
	EndNotice->SetUsingAbsoluteLocation(true);
	EndNotice->SetUsingAbsoluteRotation(true);
	EndNotice->SetUsingAbsoluteScale(true);
	EndNotice->SetWorldLocation(Location);
	EndNotice->SetWorldSize(12.f);
	// Unlit so it reads in the dark oasis regardless of eye adaptation.
	if (UMaterialInterface* TextMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_UnlitText.M_UnlitText")))
	{
		EndNotice->SetTextMaterial(TextMat);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("APiano %s: /Game/Materials/M_UnlitText missing; notice may be dark"), *GetName());
	}
	EndNotice->SetText(FText::GetEmpty());
	EndNotice->SetComponentTickEnabled(true); // face-the-player tick (it only self-enables on a visibility change)

	EndNoticeTypewriter.OnUpdate = [this](const FString& DisplayText)
	{
		EndNotice->SetText(FText::FromString(DisplayText));
	};
	EndNoticeTypewriter.Start(this, EndNoticeText, FMath::Max(0.01f, GEndNoticeCharInterval), 0.f);
	UE_LOG(LogTemp, Log, TEXT("APiano %s: end-of-demo notice shown at %s"), *GetName(), *Location.ToString());
}

UAudioComponent* APiano::GetMusic() const
{
	UAudioComponent* Music = FindComponentByClass<UStoryGatedAudioComponent>();
	if (!Music)
	{
		UE_LOG(LogTemp, Error, TEXT("APiano %s: no UStoryGatedAudioComponent (PianoMusic) on BP_Piano"), *GetName());
	}
	return Music;
}

void APiano::SetMusicPaused(bool bPaused)
{
	if (UAudioComponent* Music = GetMusic())
	{
		Music->SetPaused(bPaused);
		UE_LOG(LogTemp, Log, TEXT("APiano %s: music %s"), *GetName(), bPaused ? TEXT("paused") : TEXT("resumed"));
	}
}

bool APiano::IsMusicPaused() const
{
	const UAudioComponent* Music = GetMusic();
	return Music && Music->GetPlayState() == EAudioComponentPlayState::Paused;
}

bool APiano::IsMusicPlaying() const
{
	const UAudioComponent* Music = GetMusic();
	return Music && Music->IsPlaying() && Music->GetPlayState() != EAudioComponentPlayState::Paused;
}

FString APiano::GetEndNoticeDisplayText() const
{
	return EndNotice ? EndNotice->Text.ToString() : FString();
}
