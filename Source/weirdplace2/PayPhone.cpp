#include "PayPhone.h"

#include "StorySubsystem.h"
#include "MissingPersonPoster.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"

APayPhone::APayPhone()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APayPhone::BeginPlay()
{
	Super::BeginPlay();

	// Default placeholders: low voices ("stuff through static") + a windy hiss
	// standing in for the static bed. Real audio swapped in later.
	if (!VoiceSound)
	{
		VoiceSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/LowVoiceSoundCue.LowVoiceSoundCue"));
	}
	if (!StaticSound)
	{
		StaticSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/WindInside.WindInside"));
	}

	USceneComponent* Root = GetRootComponent();
	if (Root)
	{
		// Hidden until SeenTornadoWarning. Propagate to all child meshes/lights.
		Root->SetVisibility(false, true);

		StaticAudio = NewObject<UAudioComponent>(this, TEXT("PayPhoneStaticAudio"));
		StaticAudio->SetupAttachment(Root);
		StaticAudio->bAutoActivate = false;
		StaticAudio->SetSound(StaticSound);
		StaticAudio->RegisterComponent();

		VoiceAudio = NewObject<UAudioComponent>(this, TEXT("PayPhoneVoiceAudio"));
		VoiceAudio->SetupAttachment(Root);
		VoiceAudio->bAutoActivate = false;
		VoiceAudio->SetSound(VoiceSound);
		VoiceAudio->RegisterComponent();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("APayPhone %s: no root component (BP not set up?)"), *GetName());
	}

	if (UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr)
	{
		FlagChangedHandle = Story->OnStoryFlagChanged.AddUObject(this, &APayPhone::OnStoryFlagChanged);
		if (Story->IsFlagSet(EStoryFlag::SeenTornadoWarning))
		{
			Reveal();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("APayPhone %s: no UStorySubsystem; will stay hidden"), *GetName());
	}

	// Staple the "missing person" poster to the pole as a SEPARATE world actor
	// (NOT attached to this scene's root) so it stays present regardless of the
	// SeenTornadoWarning reveal gate.
	if (UWorld* World = GetWorld())
	{
		const FTransform ActorTM = GetActorTransform();
		const FVector PosterLoc = ActorTM.TransformPosition(PosterRelativeOffset);
		const FRotator PosterRot = GetActorRotation() + FRotator(0.f, PosterRelativeYaw, 0.f);
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Poster = World->SpawnActor<AMissingPersonPoster>(AMissingPersonPoster::StaticClass(), PosterLoc, PosterRot, SpawnParams);
		if (Poster)
		{
			UE_LOG(LogTemp, Log, TEXT("APayPhone %s: spawned missing-person poster at %s"), *GetName(), *PosterLoc.ToString());
		}
	}
}

void APayPhone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FlagChangedHandle.IsValid())
	{
		if (UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr)
		{
			Story->OnStoryFlagChanged.Remove(FlagChangedHandle);
		}
		FlagChangedHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void APayPhone::OnStoryFlagChanged(EStoryFlag Flag, bool bValue)
{
	if (Flag == EStoryFlag::SeenTornadoWarning && bValue)
	{
		Reveal();
	}
}

void APayPhone::Reveal()
{
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetVisibility(true, true);
		UE_LOG(LogTemp, Log, TEXT("APayPhone %s: revealed (SeenTornadoWarning)"), *GetName());
	}
}

void APayPhone::Interact_Implementation()
{
	UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr;
	if (!Story || !Story->IsFlagSet(EStoryFlag::SeenTornadoWarning) || bPlayedOnce)
	{
		UE_LOG(LogTemp, Log, TEXT("APayPhone %s: interact ignored (seen=%d, playedOnce=%d)"),
			*GetName(), (Story && Story->IsFlagSet(EStoryFlag::SeenTornadoWarning)) ? 1 : 0, bPlayedOnce ? 1 : 0);
		return;
	}

	if (StaticAudio)
	{
		StaticAudio->Play();
	}
	if (VoiceAudio)
	{
		VoiceAudio->Play();
	}
	bPlayedOnce = true;
	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: playing static + voices"), *GetName());
}

bool APayPhone::CanInteract()
{
	UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr;
	return Story && Story->IsFlagSet(EStoryFlag::SeenTornadoWarning) && !bPlayedOnce;
}

bool APayPhone::IsAudioPlaying() const
{
	return (StaticAudio && StaticAudio->IsPlaying()) || (VoiceAudio && VoiceAudio->IsPlaying());
}
