#include "PayPhone.h"

#include "MyCharacter.h"
#include "StorySubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

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
		// VoiceSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/LowVoiceSoundCue.LowVoiceSoundCue"));
	}
	if (!StaticSound)
	{
		// StaticSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/WindInside.WindInside"));
	}

	// Receiver SFX: pickup (one-shot) -> dialtone (looping) -> hangup (one-shot).
	if (!PickupSound)
	{
		PickupSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/Phone/phone_pickup.phone_pickup"));
	}
	if (!DialtoneSound)
	{
		DialtoneSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/Phone/phone_dialtone.phone_dialtone"));
	}
	// Placeholder for the spoken code (low voices stand in until the real "4-7-2-9"
	// recording is dropped in). Plays once over the dialtone.
	if (!CodeSound)
	{
		CodeSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/LowVoiceSoundCue.LowVoiceSoundCue"));
	}
	if (!HangupSound)
	{
		HangupSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/Phone/phone_hangup.phone_hangup"));
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

		PickupAudio = NewObject<UAudioComponent>(this, TEXT("PayPhonePickupAudio"));
		PickupAudio->SetupAttachment(Root);
		PickupAudio->bAutoActivate = false;
		PickupAudio->SetSound(PickupSound);
		PickupAudio->RegisterComponent();

		DialtoneAudio = NewObject<UAudioComponent>(this, TEXT("PayPhoneDialtoneAudio"));
		DialtoneAudio->SetupAttachment(Root);
		DialtoneAudio->bAutoActivate = false;
		DialtoneAudio->SetSound(DialtoneSound);
		DialtoneAudio->RegisterComponent();

		CodeAudio = NewObject<UAudioComponent>(this, TEXT("PayPhoneCodeAudio"));
		CodeAudio->SetupAttachment(Root);
		CodeAudio->bAutoActivate = false;
		CodeAudio->SetSound(CodeSound);
		CodeAudio->RegisterComponent();

		HangupAudio = NewObject<UAudioComponent>(this, TEXT("PayPhoneHangupAudio"));
		HangupAudio->SetupAttachment(Root);
		HangupAudio->bAutoActivate = false;
		HangupAudio->SetSound(HangupSound);
		HangupAudio->RegisterComponent();
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
}

void APayPhone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Level tearing down mid-call: don't leave the player movement-frozen or a
	// dangling "Exit Interaction" binding on the controller.
	if (bOffHook)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DialtoneStartTimer);
		}
		ReleasePlayer();
		bOffHook = false;
	}

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
	if (!Story || !Story->IsFlagSet(EStoryFlag::SeenTornadoWarning) || bOffHook)
	{
		UE_LOG(LogTemp, Log, TEXT("APayPhone %s: interact ignored (seen=%d, offHook=%d)"),
			*GetName(), (Story && Story->IsFlagSet(EStoryFlag::SeenTornadoWarning)) ? 1 : 0, bOffHook ? 1 : 0);
		return;
	}

	bOffHook = true;

	// Record that the player has used the phone at least once. Seneca's smoking
	// appearance outside gates on this flag, so it persists past hang-up.
	Story->SetFlag(EStoryFlag::UsedPayPhone, true);

	// Hold the player at the phone — freeze movement and bind hang-up. Look stays
	// free (VR owns the headset), so bFreezeLook=false.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (AMyCharacter* Character = Cast<AMyCharacter>(PC->GetPawn()))
		{
			Character->BeginInteractionHold(/*bFreezeLook*/ false);
		}
		if (PC->InputComponent)
		{
			PC->InputComponent->BindAction("Exit Interaction", IE_Pressed, this, &APayPhone::HangUp);
		}
	}

	if (PickupAudio)
	{
		PickupAudio->Play();
	}

	// Hand off to the dialtone once the pickup one-shot finishes. A timer (not
	// OnAudioFinished) — deterministic and fires under headless PIE where the
	// audio engine may not raise the finish event.
	const float PickupDuration = PickupSound ? PickupSound->GetDuration() : 0.5f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DialtoneStartTimer, this, &APayPhone::StartDialtone, PickupDuration, false);
	}

	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: picked up (pickup -> dialtone in %.2fs)"), *GetName(), PickupDuration);
}

void APayPhone::StartDialtone()
{
	if (!bOffHook)
	{
		return;
	}

	// Looping dialtone (seamless via the SoundWave's bLooping), with the
	// static + voices bleeding over it.
	if (DialtoneAudio)
	{
		DialtoneAudio->Play();
	}
	if (StaticAudio)
	{
		StaticAudio->Play();
	}
	if (VoiceAudio)
	{
		VoiceAudio->Play();
	}
	// Spoken bathroom code bleeds over the dialtone (one-shot).
	if (CodeAudio)
	{
		CodeAudio->Play();
	}
	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: dialtone looping (static + voices + code over it)"), *GetName());
}

void APayPhone::HangUp()
{
	if (!bOffHook)
	{
		return;
	}

	// Covers hanging up during the pickup, before the dialtone timer fires.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DialtoneStartTimer);
	}

	if (DialtoneAudio)
	{
		DialtoneAudio->Stop();
	}
	if (StaticAudio)
	{
		StaticAudio->Stop();
	}
	if (VoiceAudio)
	{
		VoiceAudio->Stop();
	}
	if (CodeAudio)
	{
		CodeAudio->Stop();
	}
	if (PickupAudio)
	{
		PickupAudio->Stop();
	}

	if (HangupAudio)
	{
		HangupAudio->Play();
	}

	ReleasePlayer();

	bOffHook = false;
	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: hung up"), *GetName());
}

void APayPhone::ReleasePlayer()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	if (PC->InputComponent)
	{
		PC->InputComponent->RemoveActionBinding("Exit Interaction", IE_Pressed);
	}
	if (AMyCharacter* Character = Cast<AMyCharacter>(PC->GetPawn()))
	{
		Character->EndInteractionHold(/*bUnfreezeLook*/ false);
	}
}

bool APayPhone::CanInteract()
{
	UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr;
	return Story && Story->IsFlagSet(EStoryFlag::SeenTornadoWarning) && !bOffHook;
}

bool APayPhone::IsAudioPlaying() const
{
	return (PickupAudio && PickupAudio->IsPlaying())
		|| (DialtoneAudio && DialtoneAudio->IsPlaying())
		|| (StaticAudio && StaticAudio->IsPlaying())
		|| (VoiceAudio && VoiceAudio->IsPlaying());
}

bool APayPhone::IsDialtonePlaying() const
{
	return DialtoneAudio && DialtoneAudio->IsPlaying();
}
