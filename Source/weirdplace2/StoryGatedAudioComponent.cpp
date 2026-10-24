#include "StoryGatedAudioComponent.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"

UStoryGatedAudioComponent::UStoryGatedAudioComponent()
{
	bAutoActivate = false;
}

void UStoryGatedAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	UStorySubsystem* Story = GetWorld()->GetSubsystem<UStorySubsystem>();
	if (!Story)
	{
		UE_LOG(LogTemp, Error, TEXT("StoryGatedAudioComponent on %s: no UStorySubsystem, will never play"), *GetOwner()->GetName());
		return;
	}

	if (Story->IsFlagSet(GateFlag))
	{
		StartGatedPlayback();
		return;
	}
	FlagChangedHandle = Story->OnStoryFlagChanged.AddUObject(this, &UStoryGatedAudioComponent::OnStoryFlagChanged);
}

void UStoryGatedAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FlagChangedHandle.IsValid())
	{
		if (UStorySubsystem* Story = GetWorld()->GetSubsystem<UStorySubsystem>())
		{
			Story->OnStoryFlagChanged.Remove(FlagChangedHandle);
		}
		FlagChangedHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UStoryGatedAudioComponent::OnStoryFlagChanged(EStoryFlag Flag, bool bValue)
{
	if (Flag != GateFlag || !bValue)
	{
		return;
	}
	StartGatedPlayback();
}

void UStoryGatedAudioComponent::StartGatedPlayback()
{
	if (!Sound)
	{
		UE_LOG(LogTemp, Error, TEXT("StoryGatedAudioComponent on %s: gate fired but no Sound assigned"), *GetOwner()->GetName());
		return;
	}
	if (IsPlaying())
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("StoryGatedAudioComponent on %s: flag %d set, fading in '%s' over %.1fs"),
		*GetOwner()->GetName(), static_cast<int32>(GateFlag), *Sound->GetName(), FadeInSeconds);
	FadeIn(FadeInSeconds, 1.0f, 0.0f, FadeInCurve);
}
