#include "CarRideComponent.h"
#include "Rick.h"
#include "UI_Dialogue.h"
#include "Components/WidgetComponent.h"
#include "FirstPersonCharacter.h"
#include "BladderUrgencyComponent.h"
#include "MovieBox.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif

// Dust motes drift through the headlight beam zone just ahead of the car
// (conveyor-local X; the car nose is ~2-3m ahead of the seat anchor).
static constexpr float MoteLoopMinX = 150.0f;
static constexpr float MoteLoopMaxX = 1100.0f;

// 0 = use the component's RideSpeed UPROPERTY
static float GCarRideSpeedOverride = 0.0f;
static FAutoConsoleVariableRef CVarCarRideSpeed(
	TEXT("weird.CarRide.Speed"),
	GCarRideSpeedOverride,
	TEXT("Override car-ride scenery speed in cm/s (0 = use component RideSpeed)."));

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

	// Poll until the player pawn is the right type, then start (or skip) the ride
	GetWorld()->GetTimerManager().SetTimer(
		DialogueStartTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, bSkipRide]()
		{
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

	// Hide the gas station during the ride (must iterate children; SetActorHiddenInGame doesn't propagate)
	if (GasStationRoot)
	{
		TArray<AActor*> GasStationActors;
		GasStationRoot->GetAttachedActors(GasStationActors, /*bResetArray=*/true, /*bRecursively=*/true);
		for (AActor* Actor : GasStationActors)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
		}
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

	if (bSceneryMoving && SceneryConveyor)
	{
		TickScenery(DeltaTime);
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

	// Bind to player's dialogue line delegate for bladder pulse trigger
	if (PC)
	{
		if (AFirstPersonCharacter* FPPlayer = Cast<AFirstPersonCharacter>(PC->GetPawn()))
		{
			FPPlayer->OnDialogueLineShown.AddDynamic(this, &UCarRideComponent::OnDialogueLineShown);
		}
	}

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
		PostDialogueRideTime,
		false
	);
}

void UCarRideComponent::OnDialogueLineShown(int32 LineIndex)
{
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
	// Fade camera to black
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeDuration, FLinearColor::Black, false, true);
	}

	// Schedule teleport after fade completes
	GetWorld()->GetTimerManager().SetTimer(
		FadeOutTimerHandle,
		this,
		&UCarRideComponent::OnFadeOutComplete,
		FadeDuration,
		false
	);
}

void UCarRideComponent::OnFadeOutComplete()
{
	// Stop scenery movement and tear the conveyor down while the screen is black
	bSceneryMoving = false;
	SetComponentTickEnabled(false);
	DestroyScenery();

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
		UE_LOG(LogTemp, Display, TEXT("CarRideComponent::OnFadeOutComplete - Teleport: EyeTarget=%s ActorLoc=%s CamOffset=%s Rot=%s"),
			*TargetEye.ToString(), *ActorLocation.ToString(), *CameraOffset.ToString(), *TargetRotation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::OnFadeOutComplete - ArrivalTarget is null!"));
	}

	// Show the gas station now that the ride is over
	if (GasStationRoot)
	{
		TArray<AActor*> GasStationActors;
		GasStationRoot->GetAttachedActors(GasStationActors, /*bResetArray=*/true, /*bRecursively=*/true);
		for (AActor* Actor : GasStationActors)
		{
			Actor->SetActorHiddenInGame(false);
			Actor->SetActorEnableCollision(true);
		}
	}

	// Restore all spawned movie boxes
	{
		TArray<AActor*> MovieBoxes;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMovieBox::StaticClass(), MovieBoxes);
		for (AActor* Actor : MovieBoxes)
		{
			Actor->SetActorHiddenInGame(false);
			Actor->SetActorEnableCollision(true);
		}
	}

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
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeDuration, FLinearColor::Black, false, false);
	}

	if (Rick)
	{
		// Restore widget to its original relative position and ensure it's closed
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
	GetOwner()->SetActorTransform(InitialCarTransform, false, nullptr, ETeleportType::TeleportPhysics);
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

void UCarRideComponent::TickScenery(float DeltaTime)
{
	const float Speed = GCarRideSpeedOverride > 0.0f ? GCarRideSpeedOverride : RideSpeed;
	const float Step = Speed * DeltaTime;
	for (USceneComponent* Item : ConveyorItems)
	{
		FVector Rel = Item->GetRelativeLocation();
		Rel.X -= Step;
		if (Rel.X < -BehindDistance)
		{
			Rel.X += LoopLength;
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
