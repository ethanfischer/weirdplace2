#include "CarRideComponent.h"
#include "LookAtPlayerComponent.h"
#include "Rick.h"
#include "UI_Dialogue.h"
#include "Components/WidgetComponent.h"
#include "DialoguePlateComponent.h"
#include "FirstPersonCharacter.h"
#include "BladderUrgencyComponent.h"
#include "MovieBox.h"
#include "Camera/CameraComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/AmbientSound.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Tunable.h"
#include "StorySubsystem.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif

// Dust motes drift through the headlight beam zone just ahead of the car
// (conveyor-local X; the car nose is ~2-3m ahead of the seat anchor).
static constexpr float MoteLoopMinX = 150.0f;
static constexpr float MoteLoopMaxX = 1100.0f;

WP_TUNABLE_FLOAT(GCarRideSpeedOverride, "weird.CarRide.Speed", 0.0f,
	"Override car-ride scenery speed in cm/s (0 = use component RideSpeed).");

WP_TUNABLE_FLOAT(GCarRidePostDialogueRideTime, "weird.CarRide.PostDialogueRideTime", 3.0f,
	"Seconds between Rick's last car line and the car starting its drive to the gas station.");

WP_TUNABLE_FLOAT(GCarRideStartBlackHoldOverride, "weird.CarRide.StartBlackHold", 0.0f,
	"Override ride-start pure-black hold in seconds (0 = use component RideStartBlackHoldDuration). Takes effect on next PIE.");

WP_TUNABLE_FLOAT(GCarRideStartFadeInOverride, "weird.CarRide.StartFadeIn", 0.0f,
	"Override ride-start fade-in-from-black duration in seconds (0 = use component RideStartFadeInDuration). Takes effect on next PIE.");

WP_TUNABLE_FLOAT(GCarRideMusicFadeDelayOverride, "weird.CarRide.MusicFadeDelay", -1.0f,
	"Override delay from ride start until the music fade-in begins, in seconds (<0 = use component MusicFadeInDelay). Takes effect on next PIE.");

WP_TUNABLE_FLOAT(GCarRideMusicFadeInOverride, "weird.CarRide.MusicFadeIn", 0.0f,
	"Override music fade-in duration in seconds (0 = use component MusicFadeInDuration). Takes effect on next PIE.");

WP_TUNABLE_FLOAT(GCarRideExitDipOutOverride, "weird.CarRide.ExitDipOut", 0.0f,
	"Override the dip-to-black when getting out of the parked car, in seconds (0 = use component ExitDipOutDuration).");

WP_TUNABLE_FLOAT(GCarRideExitDipInOverride, "weird.CarRide.ExitDipIn", 0.0f,
	"Override the fade-back-in after getting out of the car, in seconds (0 = use component ExitDipInDuration).");

static FAutoConsoleCommandWithWorldAndArgs GCarRideSkipToCmd(
	TEXT("weird.CarRide.SkipTo"),
	TEXT("Jump the running car ride to a stage: Dialogue (first line, screen clear), LastLine (Rick's final line showing) or Drive (dialogue done, car pulling away)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != 1)
		{
			UE_LOG(LogTemp, Error, TEXT("weird.CarRide.SkipTo <Dialogue|LastLine|Drive>"));
			return;
		}
		for (TObjectIterator<UCarRideComponent> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				It->SkipTo(FName(*Args[0]));
			}
		}
	}));

static FAutoConsoleCommandWithWorld GCarRideForceStartCmd(
	TEXT("weird.CarRide.ForceStart"),
	TEXT("Start the car ride even if PIE began from the camera location (SkipRide path)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		for (TObjectIterator<UCarRideComponent> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				It->ForceStartRide();
			}
		}
	}));

static FAutoConsoleCommandWithWorld GCarRideRebuildSceneryCmd(
	TEXT("weird.CarRide.RebuildScenery"),
	TEXT("Destroy and respawn the car-ride scenery conveyor with current property values."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		for (TObjectIterator<UCarRideComponent> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				It->RebuildScenery();
			}
		}
	}));

UCarRideComponent::UCarRideComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	PropMeshPaths = {
		TEXT("/Game/StarterContent/Props/SM_Bush.SM_Bush"),
		TEXT("/Game/StarterContent/Props/SM_Rock.SM_Rock"),
	};
}

void UCarRideComponent::BeginPlay()
{
	Super::BeginPlay();

	InitialCarTransform = GetOwner()->GetActorTransform();

	// Capture everything riding in the car (Rick + tagged props) relative to
	// it now, before SkipRide/AppearOutside can move any of them. The drive
	// re-poses them from these transforms every frame.
	Riders.Reset();
	RiderLocalLocations.Reset();
	RiderLocalRotations.Reset();
	if (Rick)
	{
		Riders.Add(Rick);
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(RiderActorTag) && *It != GetOwner())
		{
			Riders.Add(*It);
		}
	}
	for (AActor* Rider : Riders)
	{
		RiderLocalLocations.Add(InitialCarTransform.InverseTransformPosition(Rider->GetActorLocation()));
		RiderLocalRotations.Add(InitialCarTransform.GetRotation().Inverse() * Rider->GetActorQuat());
	}
	UE_LOG(LogTemp, Log, TEXT("CarRideComponent: %d riders captured"), Riders.Num());

	bool bSkipRide = false;
#if WITH_EDITOR
	if (const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>())
	{
		bSkipRide = PlaySettings->LastExecutedPlayModeLocation == PlayLocation_CurrentCameraLocation;
	}
	// Automation runs must be deterministic: never enter the ride from the
	// play-location heuristic (it reads a saved per-user editor setting).
	// The CarRideScenery test opts in via TestDriver ForceStartCarRide().
	bSkipRide |= GIsAutomationTesting;
#endif

	// Black out video and silence world audio right away on the ride path —
	// StartRide only runs once the pawn poll succeeds, and audio starting in
	// that gap would pierce the black hold. The camera fade is video-only:
	// ambients and music get per-component fades so the music timeline is
	// independent of the visual fade.
	auto ApplyStartFade = [this]()
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, /*bFadeAudio=*/false);
			SilenceWorldAudioForRideStart();
			return true;
		}
		return false;
	};
	bool bStartFadeApplied = bSkipRide || ApplyStartFade();

	// Poll until the player pawn is the right type, then start (or skip) the ride
	GetWorld()->GetTimerManager().SetTimer(
		DialogueStartTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, bSkipRide, bStartFadeApplied, ApplyStartFade]() mutable
		{
			if (!bStartFadeApplied)
			{
				bStartFadeApplied = ApplyStartFade();
			}
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (PC && Cast<AFirstPersonCharacter>(PC->GetPawn()))
			{
				if (bSkipRide)
				{
					SkipRide();
				}
				else
				{
					StartRide();
				}
			}
		}),
		0.1f,
		true
	);
}

void UCarRideComponent::SilenceWorldAudioForRideStart()
{
	SilencedAmbients.Empty();
	SilencedAmbientVolumes.Empty();
	MusicComponent = nullptr;

	for (TActorIterator<AAmbientSound> It(GetWorld()); It; ++It)
	{
		UAudioComponent* AC = It->GetAudioComponent();
		if (!AC)
		{
			continue;
		}
		// Non-auto-activated ambients (e.g. aliensound) are script-triggered
		// later — leave them untouched: silencing would mute their trigger,
		// and FadeIn would start a sound that isn't supposed to play yet.
		if (!AC->bAutoActivate)
		{
			continue;
		}
		if (It->ActorHasTag(MusicActorTag))
		{
			MusicComponent = AC;
			MusicOriginalVolume = AC->VolumeMultiplier;
		}
		else if (AC->Sound && AC->Sound->IsLooping())
		{
			SilencedAmbients.Add(AC);
			SilencedAmbientVolumes.Add(AC->VolumeMultiplier);
		}
		else
		{
			// One-shot (e.g. aliensound): silence it but don't restart it at
			// fade time — re-triggering a finished one-shot mid-ride is wrong,
			// and pre-fade it was inaudible under the old master audio duck.
			UE_LOG(LogTemp, Display, TEXT("CarRideComponent: skipping restart of one-shot ambient '%s'"), *It->GetActorNameOrLabel());
		}
		AC->SetVolumeMultiplier(0.0f);
	}

	if (!MusicComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SilenceWorldAudioForRideStart - no AmbientSound tagged '%s' found"), *MusicActorTag.ToString());
	}
	UE_LOG(LogTemp, Display, TEXT("CarRideComponent::SilenceWorldAudioForRideStart - silenced %d ambients (+music: %d)"),
		SilencedAmbients.Num(), MusicComponent ? 1 : 0);

	// Music fade-in starts immediately (plus optional MusicFadeInDelay),
	// independent of the black hold / visual fade. FadeIn restarts the sound
	// (VolumeMultiplier 0 culled it), so no is-playing dance is needed.
	if (MusicComponent)
	{
		const float MusicDelay = GCarRideMusicFadeDelayOverride >= 0.0f ? GCarRideMusicFadeDelayOverride : MusicFadeInDelay;
		GetWorld()->GetTimerManager().SetTimer(
			MusicFadeTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (!MusicComponent)
				{
					return;
				}
				const float MusicFadeIn = GCarRideMusicFadeInOverride > 0.0f ? GCarRideMusicFadeInOverride : MusicFadeInDuration;
				MusicComponent->SetVolumeMultiplier(MusicOriginalVolume);
				MusicComponent->FadeIn(MusicFadeIn, 1.0f);
			}),
			FMath::Max(MusicDelay, 0.05f),
			false
		);
	}
}

void UCarRideComponent::StartRide()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent: No PlayerController found"));
		return;
	}

	AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(PC->GetPawn());
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent: Player is not AFirstPersonCharacter"));
		return;
	}

	// Aim the staged car so its travel direction runs straight to the turn-in
	// point of the parking approach — the drive then needs no steering until
	// the pull-over. Riders (Rick, headlight beams) are re-posed with it.
	FTransform Parking;
	if (Rick && Rick->GetCarParkingTransform(Parking) && PassengerSeatTarget)
	{
		FTransform Staged = InitialCarTransform;
		const float SeatYawFromCar = PassengerSeatTarget->GetActorRotation().Yaw - GetOwner()->GetActorRotation().Yaw;
		const float ParkedTravelYaw = Parking.Rotator().Yaw + SeatYawFromCar;
		const FVector2D EndDir(FMath::Cos(FMath::DegreesToRadians(ParkedTravelYaw)), FMath::Sin(FMath::DegreesToRadians(ParkedTravelYaw)));
		const FVector2D P2(Parking.GetLocation());
		const FVector2D P1 = P2 - EndDir * DriveTurnRadius;
		const FVector2D ToP1 = (P1 - FVector2D(Staged.GetLocation())).GetSafeNormal();
		const float TravelYaw = FMath::RadiansToDegrees(FMath::Atan2(ToP1.Y, ToP1.X));
		Staged.SetRotation(FRotator(0.0f, TravelYaw - SeatYawFromCar, 0.0f).Quaternion());
		PlaceCar(Staged);
		UE_LOG(LogTemp, Display, TEXT("CarRideComponent::StartRide - staged car aimed at turn-in (travel yaw %.1f)"), TravelYaw);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::StartRide - Rick/OutsidePositionTarget/PassengerSeatTarget unassigned; car not aimed at the station"));
	}

	// The gas station stays visible all ride (it's ~400 m out in the fog and
	// unlit, so it can't be seen) — toggling 60-odd actors' visibility would
	// cost a frame of shader/shadow work exactly when the drive starts. Only
	// its lights go dark here; BeginDrive ramps them back up.
	StationLights.Reset();
	StationLightIntensities.Reset();
	if (GasStationRoot)
	{
		TArray<AActor*> GasStationActors;
		GasStationRoot->GetAttachedActors(GasStationActors, /*bResetArray=*/true, /*bRecursively=*/true);
		for (AActor* Actor : GasStationActors)
		{
			TArray<ULightComponent*> Lights;
			Actor->GetComponents<ULightComponent>(Lights);
			for (ULightComponent* Light : Lights)
			{
				if (Light->Mobility == EComponentMobility::Static)
				{
					continue; // baked; can't be ramped
				}
				StationLights.Add(Light);
				StationLightIntensities.Add(Light->Intensity);
				Light->SetIntensity(0.0f);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::StartRide - GasStationRoot is null"));
	}

	// Hide all spawned movie boxes during the ride
	{
		TArray<AActor*> MovieBoxes;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMovieBox::StaticClass(), MovieBoxes);
		for (AActor* Actor : MovieBoxes)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
		}
	}

	// Disable movement BEFORE the teleport. UE 5.7 floor-snaps in the next tick
	// after SetActorLocation while MOVE_Walking is active, which would yank the
	// player off the seat. MOVE_None disables CharacterMovementComponent physics.
	PC->SetIgnoreMoveInput(true);
	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(MOVE_None);
		MoveComp->SetJumpAllowed(false);
		MoveComp->GravityScale = 0.0f;
	}
	Player->SetCanInteract(false);

	// Teleport player to passenger seat
	// Offset down by camera relative position so the player's eye (not feet) lands at the target
	if (PassengerSeatTarget)
	{
		FVector CameraOffset = FVector::ZeroVector;
		if (UCameraComponent* Camera = Player->GetFirstPersonCamera())
		{
			CameraOffset = Camera->GetRelativeLocation();
		}
		const FVector TargetEye = PassengerSeatTarget->GetActorLocation();
		const FVector ActorLocation = TargetEye - CameraOffset;
		const FRotator TargetRotation = PassengerSeatTarget->GetActorRotation();
		Player->SetActorLocationAndRotation(ActorLocation, TargetRotation, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(TargetRotation);
		UE_LOG(LogTemp, Display, TEXT("CarRideComponent::StartRide - Teleport: EyeTarget=%s ActorLoc=%s CamOffset=%s Rot=%s"),
			*TargetEye.ToString(), *ActorLocation.ToString(), *CameraOffset.ToString(), *TargetRotation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent: PassengerSeatTarget is null!"));
	}

	// Open the ride on pure black, hold, then a long fade in. Visuals and
	// ambients share the hold+fade timeline; music runs its own delay+fade.
	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, /*bFadeAudio=*/false);
		GetWorld()->GetTimerManager().SetTimer(
			RideStartFadeTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				RevealRideStart(GCarRideStartFadeInOverride > 0.0f ? GCarRideStartFadeInOverride : RideStartFadeInDuration);
			}),
			GCarRideStartBlackHoldOverride > 0.0f ? GCarRideStartBlackHoldOverride : RideStartBlackHoldDuration,
			false
		);

	}

	// Rick stares at the road until his first line of dialogue — the seat
	// teleport puts the player inside his LookAtPlayerComponent sphere, which
	// would otherwise lock his gaze onto the player for the whole ride.
	if (ULookAtPlayerComponent* LookAt = GetRickLookAtComponent())
	{
		LookAt->SetSuppressed(true);
	}

	// Spawn the silhouette conveyor and start scenery movement
	SpawnScenery();
	bSceneryMoving = true;
	SetComponentTickEnabled(true);

	// Schedule dialogue start
	GetWorld()->GetTimerManager().SetTimer(
		DialogueStartTimerHandle,
		this,
		&UCarRideComponent::StartDialogue,
		DialogueStartDelay,
		false
	);

	// Dev: a persisted `SkipTo Car<Stage>` applies now that the ride is running
	// (persisted story beats are applied by UStorySubsystem instead).
	const FString AutoSkip = UStorySubsystem::GetAutoSkipBeat();
	if (!GIsAutomationTesting && AutoSkip.StartsWith(TEXT("Car"), ESearchCase::IgnoreCase))
	{
		const FName Stage(*AutoSkip.Mid(3));
		SkipTo(Stage);
		const FString Msg = FString::Printf(TEXT("SkipTo: skipped to 'Car%s' (type `SkipTo clear` to stop)"), *Stage.ToString());
		UE_LOG(LogTemp, Display, TEXT("%s"), *Msg);
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, Msg); }
	}

}

void UCarRideComponent::RevealRideStart(float FadeSeconds)
{
	APlayerController* FadePC = GetWorld()->GetFirstPlayerController();
	if (FadePC && FadePC->PlayerCameraManager)
	{
		FadePC->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeSeconds, FLinearColor::Black, /*bShouldFadeAudio=*/false, false);
	}
	// VolumeMultiplier was zeroed at BeginPlay, which culls the active sounds
	// entirely — so restart each one with FadeIn (it Plays internally) rather
	// than ramping a dead sound.
	for (int32 i = 0; i < SilencedAmbients.Num(); ++i)
	{
		if (SilencedAmbients[i])
		{
			SilencedAmbients[i]->SetVolumeMultiplier(SilencedAmbientVolumes[i]);
			SilencedAmbients[i]->FadeIn(FadeSeconds, 1.0f);
		}
	}
}

const TArray<FName>& UCarRideComponent::GetSkipStages()
{
	static const TArray<FName> Stages = { FName("Dialogue"), FName("LastLine"), FName("Drive") };
	return Stages;
}

bool UCarRideComponent::IsSkipStage(FName Stage)
{
	return GetSkipStages().Contains(Stage);
}

void UCarRideComponent::SkipTo(FName Stage)
{
	if (!SceneryConveyor)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SkipTo - ride is not running (PIE from the car-ride start, or run weird.CarRide.ForceStart)"));
		return;
	}
	if (bDriving)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SkipTo - already driving"));
		return;
	}
	const bool bToDialogue = Stage == FName("Dialogue");
	const bool bToLastLine = Stage == FName("LastLine");
	const bool bToDrive = Stage == FName("Drive");
	if (!bToDialogue && !bToLastLine && !bToDrive)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SkipTo - unknown stage '%s' (Dialogue | LastLine | Drive)"), *Stage.ToString());
		return;
	}

	// The ride-start black hold / fade-in and the audio fades are left alone;
	// only the dialogue is skipped.
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (!bDialogueStarted)
	{
		Timers.ClearTimer(DialogueStartTimerHandle);
		StartDialogue();
	}
	if (bToDialogue)
	{
		UE_LOG(LogTemp, Display, TEXT("CarRideComponent::SkipTo Dialogue"));
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AFirstPersonCharacter* Player = PC ? Cast<AFirstPersonCharacter>(PC->GetPawn()) : nullptr;
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SkipTo - no player"));
		return;
	}
	// LastLine: show Rick's final line; advancing it ends the dialogue and
	// the post-dialogue timer starts the drive as in normal play.
	if (bToLastLine)
	{
		Player->SkipToLastDialogueLine();
		UE_LOG(LogTemp, Display, TEXT("CarRideComponent::SkipTo LastLine"));
		return;
	}

	// Drive: end the dialogue (Rick's OnDialogueEnded -> our OnDialogueEnded
	// arms the post-dialogue timer), then start the drive right away.
	Player->SkipDialogue();
	Timers.ClearTimer(PostDialogueTimerHandle);
	UE_LOG(LogTemp, Display, TEXT("CarRideComponent::SkipTo Drive"));
	EndRide();
}

void UCarRideComponent::SkipRide()
{
	GetWorld()->GetTimerManager().ClearTimer(DialogueStartTimerHandle);
	UE_LOG(LogTemp, Log, TEXT("CarRideComponent: Skipping car ride (Spawn At Camera Location)"));
	DestroyScenery();

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AFirstPersonCharacter* Player = PC ? Cast<AFirstPersonCharacter>(PC->GetPawn()) : nullptr;
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SkipRide - No player"));
		return;
	}

	if (UBladderUrgencyComponent* BladderComp = Player->FindComponentByClass<UBladderUrgencyComponent>())
	{
		BladderComp->StartUrgency();
	}

	if (Rick)
	{
		Rick->AppearOutside();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SkipRide - Rick is null"));
	}
}

void UCarRideComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDriving)
	{
		TickDrive(DeltaTime);
	}
	else if (bSceneryMoving && SceneryConveyor)
	{
		const float Speed = GCarRideSpeedOverride > 0.0f ? GCarRideSpeedOverride : RideSpeed;
		TickScenery(Speed * DeltaTime);
	}
}

void UCarRideComponent::StartDialogue()
{
	if (!Rick)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent: Rick reference is null"));
		return;
	}

	// Bind to Rick's dialogue ended delegate
	Rick->OnRickDialogueEnded.AddDynamic(this, &UCarRideComponent::OnDialogueEnded);

	// Enable interaction so player can advance dialogue with E
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		if (AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(PC->GetPawn()))
		{
			Player->SetCanInteract(true);
		}
	}

	// Rick may be a child of GasStationRoot and was hidden with it — make him visible now
	Rick->SetActorHiddenInGame(false);

	// Move dialogue widget to windshield target so player can see it from passenger seat.
	// Cache the original relative transform so we can restore it after the ride ends.
	// Centered layout: the plate component stops steering the widget beside
	// Rick's head and the text reads centered on the windshield.
	Rick->DialoguePlate->SetLayout(EDialoguePlateLayout::Centered);
	if (DialogueWidgetTarget && Rick->DialogueWidgetComponent)
	{
		AActor* WidgetActor = Rick->DialogueWidgetComponent->GetOwner();
		if (WidgetActor && WidgetActor->GetRootComponent())
		{
			CachedWidgetRelativeLocation = WidgetActor->GetRootComponent()->GetRelativeLocation();
			CachedWidgetRelativeRotation = WidgetActor->GetRootComponent()->GetRelativeRotation();
			WidgetActor->SetActorLocationAndRotation(
				DialogueWidgetTarget->GetActorLocation(),
				DialogueWidgetTarget->GetActorRotation()
			);
		}
	}

	// Set dark text color for car ride dialogue readability
	if (Rick->DialogueWidgetComponent)
	{
		if (UUI_Dialogue* DialogueWidget = Cast<UUI_Dialogue>(Rick->DialogueWidgetComponent->GetWidget()))
		{
			DialogueWidget->SetTextColor(DialogueTextColor);
		}
	}

	// Bind to player's dialogue line delegate for bladder pulse trigger, and
	// gate dialogue advance behind looking out the windshield
	if (PC)
	{
		if (AFirstPersonCharacter* FPPlayer = Cast<AFirstPersonCharacter>(PC->GetPawn()))
		{
			FPPlayer->OnDialogueLineShown.AddDynamic(this, &UCarRideComponent::OnDialogueLineShown);
			if (PassengerSeatTarget)
			{
				FPPlayer->SetDialogueGazeGate(PassengerSeatTarget->GetActorForwardVector(), DialogueGazeMaxAngleDeg);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("CarRideComponent::StartDialogue - PassengerSeatTarget is null, gaze gate not set"));
			}
		}
	}

	bDialogueStarted = true;
	Rick->StartDialogue();
}

void UCarRideComponent::OnDialogueEnded()
{
	// Unbind so future Rick dialogues (money, idle) don't re-trigger the car-ride end sequence
	Rick->OnRickDialogueEnded.RemoveDynamic(this, &UCarRideComponent::OnDialogueEnded);

	// Disable interaction again during post-dialogue ride
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		// Unbind bladder pulse listener — car ride dialogue is over
		if (AFirstPersonCharacter* FPPlayer = Cast<AFirstPersonCharacter>(PC->GetPawn()))
		{
			FPPlayer->OnDialogueLineShown.RemoveDynamic(this, &UCarRideComponent::OnDialogueLineShown);
			// Gas-station dialogues (money, idle) are not gaze-gated
			FPPlayer->ClearDialogueGazeGate();
		}

		if (AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(PC->GetPawn()))
		{
			Player->SetCanInteract(false);
		}
	}

	// Schedule end of ride
	GetWorld()->GetTimerManager().SetTimer(
		PostDialogueTimerHandle,
		this,
		&UCarRideComponent::EndRide,
		GCarRidePostDialogueRideTime,
		false
	);
}

void UCarRideComponent::OnDialogueLineShown(int32 LineIndex)
{
	// First line of the ride dialogue: start the glance cycle
	if (!bGlanceCycleStarted)
	{
		bGlanceCycleStarted = true;
		GlanceAtPlayer();
	}

	if (!Rick || Rick->BladderPulseLineIndex == INDEX_NONE || LineIndex != Rick->BladderPulseLineIndex)
	{
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(PC->GetPawn());
	if (!Player)
	{
		return;
	}

	if (!bBladderPulseArmed)
	{
		// First broadcast: line was just displayed normally.
		// Arm the block so the next E press triggers the pulse beat instead of advancing.
		bBladderPulseArmed = true;
		Player->bBlockNextDialogueAdvance = true;
		return;
	}

	// Second broadcast: player pressed E, advance was blocked, dialogue closed.
	// Fire the pulse as its own beat, then auto-advance after it finishes.
	bBladderPulseArmed = false;
	Player->SetCanInteract(false);

	if (UBladderUrgencyComponent* BladderComp = Player->FindComponentByClass<UBladderUrgencyComponent>())
	{
		BladderComp->FireSinglePulse();

		GetWorld()->GetTimerManager().SetTimer(
			BladderPulseTimerHandle,
			this,
			&UCarRideComponent::OnBladderPulseFinished,
			BladderComp->PulseDuration,
			false
		);
	}
}

ULookAtPlayerComponent* UCarRideComponent::GetRickLookAtComponent() const
{
	if (!Rick)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::GetRickLookAtComponent - Rick is null"));
		return nullptr;
	}
	ULookAtPlayerComponent* LookAt = Rick->FindComponentByClass<ULookAtPlayerComponent>();
	if (!LookAt)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::GetRickLookAtComponent - Rick has no ULookAtPlayerComponent"));
	}
	return LookAt;
}

void UCarRideComponent::GlanceAtPlayer()
{
	ULookAtPlayerComponent* LookAt = GetRickLookAtComponent();
	if (!LookAt)
	{
		return;
	}
	LookAt->SetLookAtPlayer(true);
	GetWorld()->GetTimerManager().SetTimer(
		GlanceTimerHandle,
		this,
		&UCarRideComponent::GlanceAtRoad,
		GlanceAtPlayerDuration,
		false
	);
}

void UCarRideComponent::GlanceAtRoad()
{
	ULookAtPlayerComponent* LookAt = GetRickLookAtComponent();
	if (!LookAt)
	{
		return;
	}
	LookAt->SetLookAtPlayer(false);
	GetWorld()->GetTimerManager().SetTimer(
		GlanceTimerHandle,
		this,
		&UCarRideComponent::GlanceAtPlayer,
		FMath::FRandRange(GlanceAtRoadDurationMin, GlanceAtRoadDurationMax),
		false
	);
}

void UCarRideComponent::OnBladderPulseFinished()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(PC->GetPawn());
	if (!Player)
	{
		return;
	}

	// Re-show the dialogue widget (Close() set it to Collapsed, UpdateWithText doesn't restore visibility)
	if (Rick && Rick->DialogueWidgetComponent)
	{
		if (UUI_Dialogue* DialogueWidget = Cast<UUI_Dialogue>(Rick->DialogueWidgetComponent->GetWidget()))
		{
			DialogueWidget->SetVisibility(ESlateVisibility::Visible);
		}
	}

	// Auto-advance to show the next line ("You need to pee?")
	Player->AdvanceDialogue();
	Player->SetCanInteract(true);
}

void UCarRideComponent::EndRide()
{
	BeginDrive();
}

// The approach path: a straight run from the staged car to P0, then a
// quadratic bezier P0 -> P1 -> P2 that turns the car into Rick's parking
// spot. P2 is the spot, P1 sits DriveTurnRadius behind it along the parked
// travel direction, and P0 sits DriveTurnRadius back from P1 along the
// approach direction, so the tangent is continuous at both ends.
void UCarRideComponent::BeginDrive()
{
	if (bDriving)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::BeginDrive - already driving"));
		return;
	}
	FTransform Parking;
	if (!Rick || !Rick->GetCarParkingTransform(Parking) || !PassengerSeatTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::BeginDrive - Rick/OutsidePositionTarget/PassengerSeatTarget unassigned; cannot drive"));
		return;
	}

	const FTransform CarT = GetOwner()->GetActorTransform();
	const float SeatYawFromCar = PassengerSeatTarget->GetActorRotation().Yaw - CarT.Rotator().Yaw;
	const float ParkedTravelYaw = Parking.Rotator().Yaw + SeatYawFromCar;
	const FVector2D EndDir(FMath::Cos(FMath::DegreesToRadians(ParkedTravelYaw)), FMath::Sin(FMath::DegreesToRadians(ParkedTravelYaw)));

	DriveStart = FVector2D(CarT.GetLocation());
	DriveStartZ = CarT.GetLocation().Z;
	DriveEndZ = Parking.GetLocation().Z;
	DriveP2 = FVector2D(Parking.GetLocation());
	DriveP1 = DriveP2 - EndDir * DriveTurnRadius;
	DriveDir = (DriveP1 - DriveStart).GetSafeNormal();
	DriveP0 = DriveP1 - DriveDir * DriveTurnRadius;
	DriveStraightLength = FVector2D::DotProduct(DriveP0 - DriveStart, DriveDir);
	if (DriveStraightLength < 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::BeginDrive - staged car is inside the turn zone (straight run %.0f cm)"), DriveStraightLength);
	}
	CarYawFromTangent = CarT.Rotator().Yaw - FMath::RadiansToDegrees(FMath::Atan2(DriveDir.Y, DriveDir.X));

	// Arc-length table for the bezier so speed along the curve is uniform
	constexpr int32 Samples = 48;
	DriveCurveArcTable.Reset(Samples + 1);
	DriveCurveArcTable.Add(0.0f);
	FVector2D Prev = DriveP0;
	for (int32 i = 1; i <= Samples; ++i)
	{
		const float T = float(i) / Samples;
		const FVector2D B = FMath::Square(1.0f - T) * DriveP0 + 2.0f * (1.0f - T) * T * DriveP1 + T * T * DriveP2;
		DriveCurveArcTable.Add(DriveCurveArcTable.Last() + FVector2D::Distance(Prev, B));
		Prev = B;
	}
	DriveCurveLength = DriveCurveArcTable.Last();
	DriveS = 0.0f;
	DriveSceneryEndS = DriveStraightLength - DriveTurnRadius;
	bSceneryFrozen = false;

	// The player rides along: pose relative to the car, like Rick
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AFirstPersonCharacter* Player = PC ? Cast<AFirstPersonCharacter>(PC->GetPawn()) : nullptr;
	bPlayerRiding = Player != nullptr;
	if (Player)
	{
		PlayerLocalLocation = CarT.InverseTransformPosition(Player->GetActorLocation());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::BeginDrive - no player to carry"));
	}

	// The conveyor rides along too; its props then slide back by the distance
	// actually driven each frame, which leaves them standing still in the world.
	if (SceneryConveyor)
	{
		SceneryConveyor->AttachToActor(GetOwner(), FAttachmentTransformRules::KeepWorldTransform);
	}

	// The station has been visible (and dark) since StartRide, ~400 m out in
	// the fog; bring its lights up over StationLightRampDuration so nothing pops.
	LightRampElapsed = 0.0f;
	bLightRampActive = StationLights.Num() > 0;

	bDriving = true;
	SetComponentTickEnabled(true);
	UE_LOG(LogTemp, Display, TEXT("CarRideComponent::BeginDrive - straight %.0f cm + turn %.0f cm to parking %s; %d station lights ramping"),
		DriveStraightLength, DriveCurveLength, *Parking.GetLocation().ToString(), StationLights.Num());
}

void UCarRideComponent::EvaluateDrivePath(float S, FVector& OutLocation, FVector2D& OutTangent) const
{
	FVector2D Pos;
	if (S <= DriveStraightLength)
	{
		Pos = DriveStart + DriveDir * S;
		OutTangent = DriveDir;
	}
	else
	{
		const float U = FMath::Clamp(S - DriveStraightLength, 0.0f, DriveCurveLength);
		// Arc length -> t via the table (uniform t samples)
		int32 Hi = 1;
		while (Hi < DriveCurveArcTable.Num() - 1 && DriveCurveArcTable[Hi] < U)
		{
			++Hi;
		}
		const float SegLen = DriveCurveArcTable[Hi] - DriveCurveArcTable[Hi - 1];
		const float Frac = SegLen > KINDA_SMALL_NUMBER ? (U - DriveCurveArcTable[Hi - 1]) / SegLen : 0.0f;
		const float T = (float(Hi - 1) + Frac) / float(DriveCurveArcTable.Num() - 1);
		Pos = FMath::Square(1.0f - T) * DriveP0 + 2.0f * (1.0f - T) * T * DriveP1 + T * T * DriveP2;
		OutTangent = (2.0f * (1.0f - T) * (DriveP1 - DriveP0) + 2.0f * T * (DriveP2 - DriveP1)).GetSafeNormal();
	}
	const float Total = DriveStraightLength + DriveCurveLength;
	const float Z = FMath::Lerp(DriveStartZ, DriveEndZ, Total > KINDA_SMALL_NUMBER ? FMath::Clamp(S / Total, 0.0f, 1.0f) : 1.0f);
	OutLocation = FVector(Pos.X, Pos.Y, Z);
}

void UCarRideComponent::PlaceCar(const FTransform& CarTransform)
{
	const float OldYaw = GetOwner()->GetActorRotation().Yaw;
	GetOwner()->SetActorTransform(CarTransform, false, nullptr, ETeleportType::TeleportPhysics);
	for (int32 i = 0; i < Riders.Num(); ++i)
	{
		if (Riders[i])
		{
			Riders[i]->SetActorLocationAndRotation(
				CarTransform.TransformPosition(RiderLocalLocations[i]),
				CarTransform.GetRotation() * RiderLocalRotations[i],
				false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	if (bPlayerRiding)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		AFirstPersonCharacter* Player = PC ? Cast<AFirstPersonCharacter>(PC->GetPawn()) : nullptr;
		if (Player)
		{
			Player->SetActorLocation(CarTransform.TransformPosition(PlayerLocalLocation), false, nullptr, ETeleportType::TeleportPhysics);
			// Turn the player's view with the car so they keep looking where they were looking
			FRotator Control = PC->GetControlRotation();
			Control.Yaw += FMath::FindDeltaAngleDegrees(OldYaw, CarTransform.Rotator().Yaw);
			PC->SetControlRotation(Control);
		}
	}
}

void UCarRideComponent::TickDrive(float DeltaTime)
{
	const float Cruise = GCarRideSpeedOverride > 0.0f ? GCarRideSpeedOverride : RideSpeed;
	const float Total = DriveStraightLength + DriveCurveLength;
	const float Remaining = Total - DriveS;

	// Constant deceleration from the braking point to a stop at the spot
	const float BrakeZone = DriveCurveLength + DriveBrakingDistance;
	float Speed = Cruise;
	if (Remaining < BrakeZone)
	{
		Speed = FMath::Max(DriveMinSpeed, Cruise * FMath::Sqrt(FMath::Max(Remaining, 0.0f) / BrakeZone));
	}
	const float Step = FMath::Min(Speed * DeltaTime, Remaining);
	DriveS += Step;

	FVector Location;
	FVector2D Tangent;
	EvaluateDrivePath(DriveS, Location, Tangent);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X)) + CarYawFromTangent;
	PlaceCar(FTransform(FRotator(0.0f, Yaw, 0.0f), Location));

	// Scenery: props stand still in the world while the car is on the straight;
	// hand them to the world outright once the car starts turning.
	if (SceneryConveyor)
	{
		if (!bSceneryFrozen && DriveS >= DriveStraightLength)
		{
			FreezeSceneryProps();
		}
		TickScenery(Step);
	}

	if (bLightRampActive)
	{
		LightRampElapsed += DeltaTime;
		const float Alpha = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(LightRampElapsed / FMath::Max(StationLightRampDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f));
		for (int32 i = 0; i < StationLights.Num(); ++i)
		{
			if (StationLights[i])
			{
				StationLights[i]->SetIntensity(StationLightIntensities[i] * Alpha);
			}
		}
		bLightRampActive = Alpha < 1.0f;
	}

	if (Remaining - Step <= KINDA_SMALL_NUMBER)
	{
		OnCarParked();
	}
}

void UCarRideComponent::FreezeSceneryProps()
{
	bSceneryFrozen = true;
	for (USceneComponent* Item : ConveyorItems)
	{
		if (Item)
		{
			Item->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
}

void UCarRideComponent::OnCarParked()
{
	bDriving = false;
	bSceneryMoving = false;
	// Snap any light still mid-ramp to its authored value
	for (int32 i = 0; i < StationLights.Num(); ++i)
	{
		if (StationLights[i])
		{
			StationLights[i]->SetIntensity(StationLightIntensities[i]);
		}
	}
	bLightRampActive = false;
	UE_LOG(LogTemp, Display, TEXT("CarRideComponent::OnCarParked - at %s"), *GetOwner()->GetActorLocation().ToString());

	GetWorld()->GetTimerManager().SetTimer(
		ParkedHoldTimerHandle,
		this,
		&UCarRideComponent::ExitCar,
		ParkedHoldDuration,
		false
	);
}

void UCarRideComponent::ExitCar()
{
	// Getting out is a quick dip to black; the teleport happens under it
	const float DipOut = GCarRideExitDipOutOverride > 0.0f ? GCarRideExitDipOutOverride : ExitDipOutDuration;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, DipOut, FLinearColor::Black, false, true);
	}
	GetWorld()->GetTimerManager().SetTimer(
		ExitDipTimerHandle,
		this,
		&UCarRideComponent::OnExitDipComplete,
		DipOut,
		false
	);
}

void UCarRideComponent::OnExitDipComplete()
{
	// End the glance cycle (suppression is lifted after the teleport below,
	// once the player is out of Rick's look-at sphere)
	GetWorld()->GetTimerManager().ClearTimer(GlanceTimerHandle);
	bGlanceCycleStarted = false;

	// Tear the conveyor down and restore the movie boxes while the screen is dark
	SetComponentTickEnabled(false);
	DestroyScenery();
	bPlayerRiding = false;
	{
		TArray<AActor*> MovieBoxes;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMovieBox::StaticClass(), MovieBoxes);
		for (AActor* Actor : MovieBoxes)
		{
			Actor->SetActorHiddenInGame(false);
			Actor->SetActorEnableCollision(true);
		}
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(PC->GetPawn());
	if (!Player)
	{
		return;
	}

	// Teleport player to arrival target (offset by camera height, same as seat teleport).
	// MOVE_None + StopMovementImmediately first so the post-teleport tick doesn't floor-snap.
	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(MOVE_None);
	}
	if (ArrivalTarget)
	{
		FVector CameraOffset = FVector::ZeroVector;
		if (UCameraComponent* Camera = Player->GetFirstPersonCamera())
		{
			CameraOffset = Camera->GetRelativeLocation();
		}
		const FVector TargetEye = ArrivalTarget->GetActorLocation();
		const FVector ActorLocation = TargetEye - CameraOffset;
		const FRotator TargetRotation = ArrivalTarget->GetActorRotation();
		Player->SetActorLocationAndRotation(ActorLocation, TargetRotation, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(TargetRotation);
		UE_LOG(LogTemp, Display, TEXT("CarRideComponent::OnExitDipComplete - Teleport: EyeTarget=%s ActorLoc=%s CamOffset=%s Rot=%s"),
			*TargetEye.ToString(), *ActorLocation.ToString(), *CameraOffset.ToString(), *TargetRotation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::OnExitDipComplete - ArrivalTarget is null!"));
	}

	// Player is now at the gas station, outside Rick's look-at sphere —
	// unsuppressing resolves the look to false and normal proximity behavior
	// resumes for the outside-store beats.
	if (ULookAtPlayerComponent* LookAt = GetRickLookAtComponent())
	{
		LookAt->SetSuppressed(false);
	}

	// Safety: gate is normally cleared in OnDialogueEnded, but clear again in
	// case the ride was force-ended mid-dialogue
	Player->ClearDialogueGazeGate();

	// Re-enable movement and gravity (StartRide set MOVE_None to suppress floor-snap)
	PC->SetIgnoreMoveInput(false);
	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->SetJumpAllowed(true);
		MoveComp->GravityScale = 1.0f;
	}
	Player->SetCanInteract(true);

	// Start bladder urgency if it has delayed start
	if (UBladderUrgencyComponent* BladderComp = Player->FindComponentByClass<UBladderUrgencyComponent>())
	{
		BladderComp->StartUrgency();
	}

	// Fade camera back in
	if (PC->PlayerCameraManager)
	{
		const float DipIn = GCarRideExitDipInOverride > 0.0f ? GCarRideExitDipInOverride : ExitDipInDuration;
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, DipIn, FLinearColor::Black, false, false);
	}

	if (Rick)
	{
		// Restore widget to its original relative position and ensure it's closed
		Rick->DialoguePlate->SetLayout(EDialoguePlateLayout::BesideHead);
		if (Rick->DialogueWidgetComponent)
		{
			AActor* WidgetActor = Rick->DialogueWidgetComponent->GetOwner();
			if (WidgetActor && WidgetActor->GetRootComponent())
			{
				WidgetActor->GetRootComponent()->SetRelativeLocation(CachedWidgetRelativeLocation);
				WidgetActor->GetRootComponent()->SetRelativeRotation(CachedWidgetRelativeRotation);
			}
			if (UUI_Dialogue* DialogueWidget = Cast<UUI_Dialogue>(Rick->DialogueWidgetComponent->GetWidget()))
			{
				DialogueWidget->Close();
			}
		}
		// Rick steps out onto his outside mark; the car is already parked where
		// this puts it, so it doesn't move.
		Rick->AppearOutside();
	}
}

void UCarRideComponent::ForceStartRide()
{
	// If the natural BeginPlay path already started the ride (play-location
	// heuristic went the StartRide way), the conveyor exists — don't re-enter.
	if (SceneryConveyor)
	{
		UE_LOG(LogTemp, Log, TEXT("CarRideComponent::ForceStartRide - ride already running"));
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(DialogueStartTimerHandle);
	// SkipRide may have already parked the car at the gas station via
	// Rick->AppearOutside(); put it (and its attached targets) back so the
	// forced ride runs at the staging spot, not in front of the pumps.
	bPlayerRiding = false;
	PlaceCar(InitialCarTransform);
	StartRide();
}

void UCarRideComponent::RebuildScenery()
{
	DestroyScenery();
	SpawnScenery();
}

void UCarRideComponent::SpawnScenery()
{
	if (SceneryConveyor)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SpawnScenery - conveyor already exists"));
		return;
	}
	if (!PassengerSeatTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SpawnScenery - PassengerSeatTarget is null"));
		return;
	}

	TArray<UStaticMesh*> Meshes;
	for (const FString& Path : PropMeshPaths)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (!Mesh)
		{
			UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SpawnScenery - failed to load mesh %s"), *Path);
			return;
		}
		Meshes.Add(Mesh);
	}
	if (Meshes.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SpawnScenery - PropMeshPaths is empty"));
		return;
	}

	UStaticMesh* PoleMesh = LoadObject<UStaticMesh>(nullptr, *PoleMeshPath);
	if (!PoleMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SpawnScenery - failed to load pole mesh %s"), *PoleMeshPath);
		return;
	}

	UStaticMesh* MoteMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	if (!MoteMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SpawnScenery - failed to load Shape_Sphere for dust motes"));
		return;
	}

	// Conveyor anchored at ground level under the seat, local +X = travel-forward (seat faces the windshield)
	const FRotator TravelRotation(0.0f, PassengerSeatTarget->GetActorRotation().Yaw, 0.0f);
	const FVector Anchor = PassengerSeatTarget->GetActorLocation() + FVector(0.0f, 0.0f, GroundZOffset);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SceneryConveyor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Anchor, TravelRotation, SpawnParams);
	if (!SceneryConveyor)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SpawnScenery - failed to spawn conveyor actor"));
		return;
	}
	SceneryConveyor->Tags.Add(FName("CarRideScenery"));
	USceneComponent* ConveyorRoot = NewObject<USceneComponent>(SceneryConveyor, TEXT("ConveyorRoot"));
	ConveyorRoot->SetMobility(EComponentMobility::Movable);
	SceneryConveyor->SetRootComponent(ConveyorRoot);
	ConveyorRoot->SetWorldLocationAndRotation(Anchor, TravelRotation);
	ConveyorRoot->RegisterComponent();

	BehindDistance = LoopLength * (1.0f - ForwardBias);

	auto AddItem = [this, ConveyorRoot](UStaticMesh* Mesh, float RelX, float RelY, float RelZ, float Yaw, float Scale) -> USceneComponent*
	{
		USceneComponent* ItemRoot = NewObject<USceneComponent>(SceneryConveyor);
		ItemRoot->SetMobility(EComponentMobility::Movable);
		ItemRoot->AttachToComponent(ConveyorRoot, FAttachmentTransformRules::KeepRelativeTransform);
		ItemRoot->SetRelativeLocation(FVector(RelX, RelY, RelZ));
		ItemRoot->RegisterComponent();

		UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(SceneryConveyor);
		MeshComp->SetMobility(EComponentMobility::Movable);
		MeshComp->SetStaticMesh(Mesh);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetCastShadow(false);
		MeshComp->AttachToComponent(ItemRoot, FAttachmentTransformRules::KeepRelativeTransform);
		MeshComp->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
		MeshComp->SetWorldScale3D(FVector(Scale));
		MeshComp->RegisterComponent();
		return ItemRoot;
	};

	FRandomStream Rand(RandomSeed);

	// Vegetation: sparse, both sides, randomized
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float SideSign = (Side == 0) ? 1.0f : -1.0f;
		for (int32 i = 0; i < PropsPerSide; ++i)
		{
			// Even X distribution with jitter so props never clump into a visible gap
			const float SlotLength = LoopLength / PropsPerSide;
			const float RelX = i * SlotLength + Rand.FRandRange(0.0f, SlotLength) - BehindDistance;
			const float RelY = SideSign * Rand.FRandRange(PropMinLateral, PropMaxLateral);
			ConveyorItems.Add(AddItem(Meshes[Rand.RandRange(0, Meshes.Num() - 1)], RelX, RelY, 0.0f,
				Rand.FRandRange(0.0f, 360.0f), Rand.FRandRange(PropMinScale, PropMaxScale)));
		}
	}

	// Telephone poles: regular rhythm, one side, aligned with the road
	const int32 PoleCount = FMath::Max(1, FMath::RoundToInt32(LoopLength / PoleSpacing));
	for (int32 i = 0; i < PoleCount; ++i)
	{
		ConveyorItems.Add(AddItem(PoleMesh, i * (LoopLength / PoleCount) - BehindDistance, PoleLateral, 0.0f, 0.0f, 1.0f));
	}

	// Dust motes: tiny spheres inside the headlight beam zone ahead of the car,
	// recycled on their own short loop so one is always drifting through
	for (int32 i = 0; i < DustMoteCount; ++i)
	{
		MoteItems.Add(AddItem(MoteMesh,
			Rand.FRandRange(MoteLoopMinX, MoteLoopMaxX),
			Rand.FRandRange(-250.0f, 250.0f),
			Rand.FRandRange(30.0f, 140.0f), // beam height above the road surface
			0.0f,
			Rand.FRandRange(0.008f, 0.02f)));
	}

	UE_LOG(LogTemp, Display, TEXT("CarRideComponent::SpawnScenery - spawned %d props (%d poles) + %d motes at %s"),
		ConveyorItems.Num(), PoleCount, MoteItems.Num(), *Anchor.ToString());
}

void UCarRideComponent::DestroyScenery()
{
	ConveyorItems.Empty();
	MoteItems.Empty();
	if (SceneryConveyor)
	{
		SceneryConveyor->Destroy();
		SceneryConveyor = nullptr;
	}
}

void UCarRideComponent::TickScenery(float Step)
{
	// Once the props are handed to the world (bSceneryFrozen) only the motes keep cycling
	for (int32 i = 0; i < (bSceneryFrozen ? 0 : ConveyorItems.Num()); ++i)
	{
		USceneComponent* Item = ConveyorItems[i];
		FVector Rel = Item->GetRelativeLocation();
		Rel.X -= Step;
		if (Rel.X < -BehindDistance)
		{
			Rel.X += LoopLength;
			// During the drive, a prop recycled to a spot past the turn-in would
			// stand in the gas station forecourt — it's behind the player as it
			// wraps, so just leave it hidden.
			if (bDriving && DriveS + Rel.X > DriveSceneryEndS)
			{
				Item->SetVisibility(false, true);
			}
		}
		Item->SetRelativeLocation(Rel);

		// Shadows only while inside the headlight window ahead of the car;
		// off elsewhere so the recycled fleet doesn't bloat the VSM budget.
		if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Item->GetChildComponent(0)))
		{
			const bool bInBeam = Rel.X > ShadowWindowMinX && Rel.X < ShadowWindowMaxX;
			if (MeshComp->CastShadow != bInBeam)
			{
				MeshComp->SetCastShadow(bInBeam);
			}
		}
	}

	// Motes wrap on their own short loop inside the beam zone. Each wrap
	// respawns the mote at a fresh random spot (not a fixed orbit) so a low
	// DustMoteCount doesn't read as the same particles cycling forever.
	for (USceneComponent* Mote : MoteItems)
	{
		FVector Rel = Mote->GetRelativeLocation();
		Rel.X -= Step;
		if (Rel.X < MoteLoopMinX)
		{
			Rel.X = FMath::FRandRange(MoteLoopMinX + 0.5f * (MoteLoopMaxX - MoteLoopMinX), MoteLoopMaxX);
			Rel.Y = FMath::FRandRange(-250.0f, 250.0f);
			Rel.Z = FMath::FRandRange(30.0f, 140.0f);
		}
		Mote->SetRelativeLocation(Rel);
	}
}
