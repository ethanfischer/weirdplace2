#include "PayPhone.h"

#include "CableComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FirstPersonCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "StorySubsystem.h"
#include "TimerManager.h"

APayPhone::APayPhone()
{
	// Ticks from spawn for forced-perspective scaling; with perspective off,
	// tick is enabled on demand for the receiver animation only.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
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
	// Bed under the typewriter code text; starts with the reveal in PlayCodeOnce.
	if (!CodeSound)
	{
		CodeSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/Phone/announcement.announcement"));
	}
	if (!HangupSound)
	{
		HangupSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/Phone/phone_hangup.phone_hangup"));
	}
	if (CodeVoiceChunks.Num() == 0)
	{
		for (int32 i = 0; ; ++i)
		{
			const FString Path = FString::Printf(TEXT("/Game/Sounds/Phone/VoiceChunks/announcement_voice_%03d.announcement_voice_%03d"), i, i);
			USoundBase* Chunk = LoadObject<USoundBase>(nullptr, *Path);
			if (!Chunk)
			{
				break;
			}
			CodeVoiceChunks.Add(Chunk);
		}
		if (CodeVoiceChunks.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("APayPhone %s: no voice chunks found under /Game/Sounds/Phone/VoiceChunks"), *GetName());
		}
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

	// The BP is authored Static, but Static components silently ignore runtime
	// transforms — everything must be Movable for the perspective scaling.
	TArray<USceneComponent*> SceneComps;
	GetComponents<USceneComponent>(SceneComps);
	for (USceneComponent* Comp : SceneComps)
	{
		Comp->SetMobility(EComponentMobility::Movable);
	}

	// Query-only collision everywhere: interaction gaze traces still hit, but
	// the forced-perspective scale-up can never physically block the car/pawn.
	TArray<UPrimitiveComponent*> PrimComps;
	GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* Prim : PrimComps)
	{
		Prim->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// Cache the authored light radii — attenuation is absolute cm, so it must
	// be scaled alongside the actor in UpdateForcedPerspective.
	GetComponents<ULocalLightComponent>(PerspectiveLights);
	for (const ULocalLightComponent* Light : PerspectiveLights)
	{
		PerspectiveLightBaseRadii.Add(Light->AttenuationRadius);
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
		CodeAudio->SetVolumeMultiplier(2.0f);
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

	// The receiver cord: a simulated cable from the kiosk to the receiver. The
	// baked curly cord was removed from the body mesh, so this IS the cord in
	// every state — it drapes to the cradle on-hook and stretches to the player
	// while held.
	CordCable = NewObject<UCableComponent>(this, TEXT("PayPhoneCord"));
	CordCable->SetupAttachment(KioskMesh);
	CordCable->SetRelativeLocation(CordAnchorOffset);
	// Attach the end by reflected property name, NOT SetAttachEndToComponent:
	// the raw component pointer is dropped whenever the cable re-resolves its
	// FComponentReference (re-register, solver param change), which snapped the
	// cord to the actor root underground. The property path survives re-resolution.
	CordCable->AttachEndTo.OtherActor = this;
	CordCable->AttachEndTo.ComponentProperty = FName("ReceiverMesh");
	CordCable->EndLocation = FVector(0.f, 0.f, -11.f); // handset bottom
	CordCable->CableLength = CordLength;
	CordCable->CableWidth = CordWidth;
	CordCable->NumSegments = 20;
	// The default single solver iteration lets gravity stretch the cable to ~2x
	// its rest length (measured 76cm for a 38cm cord) — that was the cord
	// sinking through the booth shelf. More iterations keep it near rest length.
	CordCable->SolverIterations = 16;
	// The default cable material renders unlit white — use the dedicated dark
	// cord material (near-black plastic).
	if (UMaterialInterface* CordMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CreatedMaterials/M_PhoneCord.M_PhoneCord")))
	{
		CordCable->SetMaterial(0, CordMat);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("APayPhone %s: M_PhoneCord missing — cord will render white"), *GetName());
	}
	CordCable->RegisterComponent();
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
	// The typewriter reveal drives the pacing (lines chain with CodeLineDelay
	// pauses); unlock hang-up only once the last character has landed, plus a
	// short beat to let it be read.
	CodeFullText.ParseIntoArrayLines(CodeLines);
	CodeLineIndex = 0;

	if (CodeAudio)
	{
		CodeAudio->Play();
	}

	int32 TotalChars = 0;
	for (const FString& Line : CodeLines)
	{
		TotalChars += Line.Len();
	}
	const float CodeDuration = TotalChars * FMath::Max(0.01f, CodeCharInterval)
		+ FMath::Max(0, CodeLines.Num() - 1) * CodeLineDelay + 1.0f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CodeEndTimer, this, &APayPhone::OnCodeFinished, CodeDuration, false);
	}

	StartNextCodeLine();

	UE_LOG(LogTemp, Log, TEXT("APayPhone %s: speaking the bathroom code (%d lines, finishes in %.2fs)"), *GetName(), CodeLines.Num(), CodeDuration);
}

void APayPhone::StartNextCodeLine()
{
	if (!CodeTextRender || !CodeLines.IsValidIndex(CodeLineIndex))
	{
		return;
	}

	CodeTypewriter.OnUpdate = [this](const FString& DisplayText)
	{
		CodeTextRender->SetText(FText::FromString(DisplayText));
	};
	// A random voice-babble syllable per typed character.
	CodeTypewriter.OnCharacterRevealed = [this](TCHAR NewChar)
	{
		if (!FChar::IsWhitespace(NewChar) && CodeVoiceChunks.Num() > 0)
		{
			USoundBase* Chunk = CodeVoiceChunks[FMath::RandRange(0, CodeVoiceChunks.Num() - 1)];
			UGameplayStatics::PlaySound2D(GetWorld(), Chunk, CodeBabbleVolume, FMath::RandRange(0.95f, 1.05f));
		}
	};
	// The finished line lingers through the pause; the next line's first
	// character replaces it.
	CodeTypewriter.OnFinished = [this]()
	{
		++CodeLineIndex;
		if (CodeLines.IsValidIndex(CodeLineIndex))
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(CodeLineTimer, this, &APayPhone::StartNextCodeLine, FMath::Max(0.02f, CodeLineDelay), false);
			}
		}
	};
	CodeTypewriter.Start(this, CodeLines[CodeLineIndex], FMath::Max(0.01f, CodeCharInterval), 0.0f);
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

	if (bEnableForcedPerspective)
	{
		UpdateForcedPerspective(DeltaSeconds);
	}
	else if (!bReceiverAnimating)
	{
		SetActorTickEnabled(false);
	}
}

void APayPhone::UpdateForcedPerspective(float DeltaTime)
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const float Dist = FVector::Dist(GetActorLocation(), Pawn->GetActorLocation());

	// Far away and already settled at max scale — skip the interp/set work.
	if (Dist > TrueScaleDistance * PerspectiveMaxScale * 2.0f && FMath::IsNearlyEqual(CurrentPerspectiveScale, PerspectiveMaxScale, 0.001f))
	{
		return;
	}

	// (distance ratio)^strength: at strength 1 the on-screen size holds exactly
	// constant during the approach; below 1 the phone still grows, just slower
	// than true perspective, so the player keeps a sense of progress. The
	// smoothstep eases the curve in over [1x, 2x] TrueScaleDistance — without
	// it the growth rate steps at the seam where the illusion hands off to
	// true perspective, which reads as a sudden speed-up near the phone.
	const float DistRatio = Dist / FMath::Max(1.0f, TrueScaleDistance);
	const float Raw = FMath::Pow(DistRatio, PerspectiveStrength);
	const float Blend = FMath::SmoothStep(1.0f, 10.0f, DistRatio);
	const float Target = FMath::Clamp(FMath::Lerp(1.0f, Raw, Blend), 1.0f, PerspectiveMaxScale);
	CurrentPerspectiveScale = FMath::FInterpTo(CurrentPerspectiveScale, Target, DeltaTime, PerspectiveInterpSpeed);
	SetActorScale3D(FVector(CurrentPerspectiveScale));

	for (int32 i = 0; i < PerspectiveLights.Num(); ++i)
	{
		PerspectiveLights[i]->SetAttenuationRadius(PerspectiveLightBaseRadii[i] * CurrentPerspectiveScale * PerspectiveLightRadiusMultiplier);
	}
}

void APayPhone::ResetCodeText()
{
	CodeTypewriter.Stop();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CodeLineTimer);
	}
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
