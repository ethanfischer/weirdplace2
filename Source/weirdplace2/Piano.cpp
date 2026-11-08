#include "Piano.h"
#include "Components/SceneComponent.h"
#include "StoryGatedAudioComponent.h"

APiano::APiano()
{
	PianistSpot = CreateDefaultSubobject<USceneComponent>(TEXT("PianistSpot"));
	PianistSpot->SetupAttachment(GetRootComponent());
	// Piano-local: the keyboard faces -X here (the level piano is turned 180°).
	PianistSpot->SetRelativeLocation(FVector(-52.f, 137.f, -65.f));
	PianistSpot->SetRelativeRotation(FRotator(0.f, 119.f, 0.f));
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
