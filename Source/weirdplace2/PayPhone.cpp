#include "PayPhone.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FirstPersonCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "StorySubsystem.h"
#include "TimerManager.h"

APayPhone::APayPhone()
{
	// Ticks only while the receiver is animating (enabled on demand).
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
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

	// Receiver SFX: pickup (one-shot) -> call audio -> hangup (one-shot).
	if (!PickupSound)
	{
		PickupSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/Phone/phone_pickup.phone_pickup"));
	}
	if (!DialtoneSound)
	{
		DialtoneSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/Phone/phone_dialtone.phone_dialtone"));
	}
	// Placeholder for the spoken code (low voices stand in until the real
	// recording is dropped in). Plays once, immediately after a short beat.
	if (!CodeSound)
	{
		CodeSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/LowVoiceSoundCue.LowVoiceSoundCue"));
	}
	if (!HangupSound)
	{
		HangupSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/Phone/phone_hangup.phone_hangup"));
	}
	if (!BodyMesh)
	{
		BodyMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fab/SM_Payphone_Body.SM_Payphone_Body"));
	}
	if (!HandsetMesh)
	{
		HandsetMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fab/SM_Payphone_Handset.SM_Payphone_Handset"));
	}

	// The diegetic code text authored in BP_TelephoneScene: cache the full line
	// and blank it — it types on in sync with the spoken code (PlayCodeOnce).
	TArray<UTextRenderComponent*> TextComps;
	GetComponents<UTextRenderComponent>(TextComps);
	for (UTextRenderComponent* Comp : TextComps)
	{
		if (Comp->GetName().Contains(TEXT("DiegeticText")))
		{
			CodeTextRender = Comp;
			CodeFullText = Comp->Text.ToString();
			Comp->SetText(FText::GetEmpty());
			break;
		}
	}
	if (!CodeTextRender)
	{
		UE_LOG(LogTemp, Error, TEXT("APayPhone %s: no DiegeticText component — code text will not display"), *GetName());
	}

	USceneComponent* Root = GetRootComponent();
	if (Root)
	{
		SetUpReceiver();

		// Hidden until SeenTornadoWarning. Propagate to all child meshes/lights
		// (including the receiver spawned above).
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

		// Busy tone (supplied later — BusySound may be null, which plays nothing).
		BusyAudio = NewObject<UAudioComponent>(this, TEXT("PayPhoneBusyAudio"));
		BusyAudio->SetupAttachment(Root);
		BusyAudio->bAutoActivate = false;
		BusyAudio->SetSound(BusySound);
		BusyAudio->RegisterComponent();

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

void APayPhone::SetUpReceiver()
{
	if (!BodyMesh || !HandsetMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("APayPhone %s: body/handset meshes missing — receiver stays baked into the kiosk"), *GetName());
		return;
	}

	// Find the authored kiosk mesh (the payphone, not the telephone pole).
	TArray<UStaticMeshComponent*> MeshComps;
	GetComponents<UStaticMeshComponent>(MeshComps);
	for (UStaticMeshComponent* Comp : MeshComps)
	{
		if (Comp->GetStaticMesh() && Comp->GetStaticMesh()->GetName().Contains(TEXT("Payphone")))
		{
			KioskMesh = Comp;
			break;
		}
	}
	if (!KioskMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("APayPhone %s: no kiosk mesh component found — receiver stays baked into the kiosk"), *GetName());
		return;
	}

	KioskMesh->SetStaticMesh(BodyMesh);

	ReceiverMesh = NewObject<UStaticMeshComponent>(this, TEXT("PayPhoneReceiver"));
	ReceiverMesh->SetStaticMesh(HandsetMesh);
	ReceiverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReceiverMesh->SetupAttachment(KioskMesh);
	ReceiverMesh->SetRelativeLocation(ReceiverCradleOffset);
	ReceiverMesh->RegisterComponent();
}

bool APayPhone::IsComponentInteractable(const UPrimitiveComponent* Component)
{
	// Only the kiosk answers. If the mesh split failed (KioskMesh null), fall
	// back to the whole actor rather than making the phone uninteractable.
	return !KioskMesh || Component == KioskMesh;
}

void APayPhone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Level tearing down mid-call: don't leave the player movement-frozen or a
	// dangling "Exit Interaction" binding on the controller.
	if (bOffHook)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CallStartTimer);
			World->GetTimerManager().ClearTimer(CodeSpeechTimer);
			World->GetTimerManager().ClearTimer(CodeEndTimer);
			World->GetTimerManager().ClearTimer(BusyToneTimer);
		}
		ResetCodeText();
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

	// First call: the player is held on the line until the spoken code has
	// fully played — the beat the phone exists for can't be skipped.
	bHangupLocked = !bCodeSpoken;

	// Record that the player has used the phone at least once. Seneca's smoking
	// appearance outside gates on this flag, so it persists past hang-up.
	Story->SetFlag(EStoryFlag::UsedPayPhone, true);

	// Hold the player at the phone — freeze movement and bind hang-up. Look stays
	// free (VR owns the headset), so bFreezeLook=false.
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	AFirstPersonCharacter* Character = PC ? Cast<AFirstPersonCharacter>(PC->GetPawn()) : nullptr;
	if (Character)
	{
		Character->BeginInteractionHold(/*bFreezeLook*/ false);
	}
	if (PC && PC->InputComponent)
	{
		PC->InputComponent->BindAction("Exit Interaction", IE_Pressed, this, &APayPhone::HangUp);
	}

	// Lift the receiver to the player's ear: reparent onto the camera (keeping
	// its world pose) and interp to the held pose in camera space.
	if (ReceiverMesh && Character && Character->GetFirstPersonCamera())
	{
		ReceiverMesh->AttachToComponent(Character->GetFirstPersonCamera(), FAttachmentTransformRules::KeepWorldTransform);
		StartReceiverAnim(ReceiverEarOffset, ReceiverEarRotation);
	}

	if (PickupAudio)
	{
		PickupAudio->Play();
	}

	// Hand off to the call audio once the pickup one-shot finishes. A timer (not
	// OnAudioFinished) — deterministic and fires under headless PIE where the
	// audio engine may not raise the finish event.
	const float PickupDuration = PickupSound ? PickupSound->GetDuration() : 0.5f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CallStartTimer, this, &APayPhone::StartCall, PickupDuration, false);
	}

	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: picked up (call audio in %.2fs, hangupLocked=%d)"),
		*GetName(), PickupDuration, bHangupLocked ? 1 : 0);
}

void APayPhone::StartCall()
{
	if (!bOffHook)
	{
		return;
	}

	// Later calls are mundane: just the looping dialtone, nothing else.
	if (bCodeSpoken)
	{
		if (DialtoneAudio)
		{
			DialtoneAudio->Play();
		}
		UE_LOG(LogTemp, Log, TEXT("APayPhone %s: dialtone only (code already heard)"), *GetName());
		return;
	}

	// First call: no dialtone — the line is already "live". Static + voices come
	// up immediately, and the spoken code follows after a short beat.
	if (StaticAudio)
	{
		StaticAudio->Play();
	}
	if (VoiceAudio)
	{
		VoiceAudio->Play();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CodeSpeechTimer, this, &APayPhone::PlayCodeOnce, FMath::Max(0.02f, CodeSpeechDelay), false);
	}
	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: first call — static+voices up, code at %.2fs"), *GetName(), CodeSpeechDelay);
}

void APayPhone::PlayCodeOnce()
{
	if (!bOffHook)
	{
		return;
	}
	if (CodeAudio)
	{
		CodeAudio->Play();
	}

	// Unlock hang-up only once the code has FULLY played. Looping placeholder
	// cues report an indefinite duration — clamp so the player is never locked
	// to the phone forever.
	float CodeDuration = CodeSound ? CodeSound->GetDuration() : 0.0f;
	if (CodeDuration <= 0.0f || CodeDuration >= INDEFINITELY_LOOPING_DURATION)
	{
		UE_LOG(LogTemp, Warning, TEXT("APayPhone %s: CodeSound duration unusable (%.1f) — treating as 4s"), *GetName(), CodeDuration);
		CodeDuration = 4.0f;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CodeEndTimer, this, &APayPhone::OnCodeFinished, CodeDuration, false);
	}

	// Type the diegetic code text on in sync with the audio: the last character
	// lands as the spoken code finishes.
	if (CodeTextRender && !CodeFullText.IsEmpty())
	{
		CodeTypewriter.OnUpdate = [this](const FString& DisplayText)
		{
			CodeTextRender->SetText(FText::FromString(DisplayText));
		};
		CodeTypewriter.StartWithDuration(this, CodeFullText, CodeDuration);
	}

	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: speaking the bathroom code (finishes in %.2fs)"), *GetName(), CodeDuration);
}

void APayPhone::OnCodeFinished()
{
	if (!bOffHook)
	{
		return;
	}
	bCodeSpoken = true;
	bHangupLocked = false;

	// A few seconds after the code, the line drops to a busy tone.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BusyToneTimer, this, &APayPhone::PlayBusyTone, FMath::Max(0.02f, BusyToneDelay), false);
	}
	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: code finished — hang-up unlocked, busy tone in %.2fs"), *GetName(), BusyToneDelay);
}

void APayPhone::PlayBusyTone()
{
	if (!bOffHook)
	{
		return;
	}
	if (BusyAudio)
	{
		BusyAudio->Play();
	}
	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: busy tone"), *GetName());
}

void APayPhone::HangUp()
{
	if (!bOffHook)
	{
		return;
	}
	if (bHangupLocked)
	{
		UE_LOG(LogTemp, Log, TEXT("APayPhone %s: hang-up refused (code still playing)"), *GetName());
		return;
	}

	// Covers hanging up at any point before pending timers fire (no stray busy
	// tone after hangup).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CallStartTimer);
		World->GetTimerManager().ClearTimer(CodeSpeechTimer);
		World->GetTimerManager().ClearTimer(CodeEndTimer);
		World->GetTimerManager().ClearTimer(BusyToneTimer);
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
	if (BusyAudio)
	{
		BusyAudio->Stop();
	}
	if (PickupAudio)
	{
		PickupAudio->Stop();
	}

	if (HangupAudio)
	{
		HangupAudio->Play();
	}

	// The code text lives only for the call — like an NPC dialogue line closing.
	ResetCodeText();

	// Return the receiver to the cradle: reparent back onto the kiosk (keeping
	// its world pose) and interp home.
	if (ReceiverMesh && KioskMesh)
	{
		ReceiverMesh->AttachToComponent(KioskMesh, FAttachmentTransformRules::KeepWorldTransform);
		StartReceiverAnim(ReceiverCradleOffset, FRotator::ZeroRotator);
	}

	ReleasePlayer();

	bOffHook = false;

	// Hanging up arms the station-relight countdown (UStorySubsystem). Hang-up
	// is locked until the code finishes, so this can't fire before the beat lands.
	if (UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr)
	{
		Story->SetFlag(EStoryFlag::HungUpPhone);
	}

	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: hung up"), *GetName());
}

void APayPhone::StartReceiverAnim(const FVector& TargetLocation, const FRotator& TargetRotation)
{
	bReceiverAnimating = true;
	ReceiverAnimElapsed = 0.0f;
	ReceiverAnimStartLoc = ReceiverMesh->GetRelativeLocation();
	ReceiverAnimStartRot = ReceiverMesh->GetRelativeRotation();
	ReceiverAnimTargetLoc = TargetLocation;
	ReceiverAnimTargetRot = TargetRotation;
	SetActorTickEnabled(true);
}

void APayPhone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bReceiverAnimating && ReceiverMesh)
	{
		ReceiverAnimElapsed += DeltaSeconds;
		const float Alpha = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(ReceiverAnimElapsed / FMath::Max(0.01f, ReceiverAnimDuration), 0.0f, 1.0f));
		ReceiverMesh->SetRelativeLocation(FMath::Lerp(ReceiverAnimStartLoc, ReceiverAnimTargetLoc, Alpha));
		ReceiverMesh->SetRelativeRotation(FQuat::Slerp(ReceiverAnimStartRot.Quaternion(), ReceiverAnimTargetRot.Quaternion(), Alpha));

		if (Alpha >= 1.0f)
		{
			bReceiverAnimating = false;
		}
	}

	if (!bReceiverAnimating)
	{
		SetActorTickEnabled(false);
	}
}

void APayPhone::ResetCodeText()
{
	CodeTypewriter.Stop();
	if (CodeTextRender)
	{
		CodeTextRender->SetText(FText::GetEmpty());
	}
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
	if (AFirstPersonCharacter* Character = Cast<AFirstPersonCharacter>(PC->GetPawn()))
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
