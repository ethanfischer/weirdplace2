#include "TestDriverSubsystem.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "ItemDefinition.h"
#include "HeldItemComponent.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#endif
#include "MovieBox.h"
#include "PropActor.h"
#include "SpawnerActorComponent.h"
#include "GazeRewardComponent.h"
#include "Hudson.h"
#include "Rick.h"
#include "Seneca.h"
#include "TestWaypoint.h"
#include "UI_Dialogue.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputKeyEventArgs.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "WeirdplaceGameUserSettings.h"

AFirstPersonCharacter* UTestDriverSubsystem::GetPlayer() const
{
	return Cast<AFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
}

bool UTestDriverSubsystem::IsPlayerReady() const
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player)
	{
		return false;
	}
	return Player->GetController() != nullptr && Player->GetFirstPersonCamera() != nullptr;
}

bool UTestDriverSubsystem::TeleportPlayerToWaypoint(FName WaypointTag)
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::TeleportPlayerToWaypoint - no player"));
		return false;
	}

	ATestWaypoint* Waypoint = ATestWaypoint::FindByTag(this, WaypointTag);
	if (!Waypoint)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::TeleportPlayerToWaypoint - no waypoint with tag '%s'"), *WaypointTag.ToString());
		return false;
	}

	const FVector RawTarget = Waypoint->GetActorLocation();
	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Target = RawTarget + FVector(0.f, 0.f, HalfHeight);
	const FRotator TargetRot = Waypoint->GetActorRotation();

	Player->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);

	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		PC->SetControlRotation(TargetRot);
	}

	UE_LOG(LogTemp, Log, TEXT("TestDriver::TeleportPlayerToWaypoint - '%s' raw=%s snapped=%s"),
		*WaypointTag.ToString(), *RawTarget.ToString(), *Target.ToString());
	return true;
}

bool UTestDriverSubsystem::TeleportNearActor(AActor* Target, float Distance)
{
	if (!Target)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::TeleportNearActor - null target"));
		return false;
	}

	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::TeleportNearActor - no player"));
		return false;
	}

	const FVector TargetLoc = Target->GetActorLocation();
	const FVector PlayerLoc = Player->GetActorLocation();

	// Direction from target to player (we'll place the player along this line).
	FVector Dir = (PlayerLoc - TargetLoc).GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector::ForwardVector;
	}

	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector NewLoc = TargetLoc + Dir * Distance + FVector(0.f, 0.f, HalfHeight);
	Player->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);

	// Face the target.
	const FRotator LookRot = (TargetLoc - NewLoc).Rotation();
	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		PC->SetControlRotation(LookRot);
	}

	UE_LOG(LogTemp, Log, TEXT("TestDriver::TeleportNearActor - near %s at %s"), *Target->GetName(), *NewLoc.ToString());
	return true;
}

bool UTestDriverSubsystem::LookAt(AActor* Target)
{
	if (!Target)
	{
		return false;
	}

	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player)
	{
		return false;
	}

	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC)
	{
		return false;
	}

	UCameraComponent* Camera = Player->GetFirstPersonCamera();
	if (!Camera)
	{
		return false;
	}

	// For MetaHuman NPCs the actor root sits at world Z=0, so aim at the
	// "Body" skeletal mesh component which reflects the rendered position.
	// Falls back to the full bounds for non-MetaHumans (props, MovieBoxes).
	FVector AimPoint;
	if (USkeletalMeshComponent* Face = Cast<USkeletalMeshComponent>(Target->GetDefaultSubobjectByName(TEXT("Face"))))
	{
		AimPoint = Face->Bounds.Origin;
	}
	else
	{
		AimPoint = Target->GetComponentsBoundingBox(/*bNonColliding*/ true).GetCenter();
	}

	const FVector CamLoc = Camera->GetComponentLocation();
	const FVector Dir = (AimPoint - CamLoc).GetSafeNormal();
	const FRotator NewRot = Dir.Rotation();

	PC->SetControlRotation(NewRot);
	UE_LOG(LogTemp, Log, TEXT("TestDriver::LookAt - %s cam=%s aim=%s rot=%s dist=%.1f"),
		*Target->GetName(), *CamLoc.ToString(), *AimPoint.ToString(),
		*NewRot.ToString(), FVector::Dist(AimPoint, CamLoc));
	return true;
}

bool UTestDriverSubsystem::LookAtActorByLabel(const FString& Label)
{
	if (AActor* Found = FindActorByLabel(Label))
	{
		return LookAt(Found);
	}

	UE_LOG(LogTemp, Error, TEXT("TestDriver::LookAtActorByLabel - no actor with label '%s'"), *Label);
	return false;
}

bool UTestDriverSubsystem::LookAtActorComponentByName(const FString& ActorLabel, const FString& ComponentName)
{
	AActor* Actor = FindActorByLabel(ActorLabel);
	if (!Actor)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::LookAtActorComponentByName - no actor '%s'"), *ActorLabel);
		return false;
	}
	return LookAtComponentByName(Actor, ComponentName);
}

bool UTestDriverSubsystem::LookAtComponentByName(AActor* Actor, const FString& ComponentName)
{
	if (!Actor)
	{
		return false;
	}

	USceneComponent* Found = FindComponentOnActorByName(Actor, ComponentName);
	if (!Found)
	{
		return false;
	}

	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player) { return false; }
	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC) { return false; }
	UCameraComponent* Camera = Player->GetFirstPersonCamera();
	if (!Camera) { return false; }

	const FVector AimPoint = Found->Bounds.Origin;
	const FVector CamLoc = Camera->GetComponentLocation();
	const FRotator NewRot = (AimPoint - CamLoc).GetSafeNormal().Rotation();
	PC->SetControlRotation(NewRot);

	UE_LOG(LogTemp, Log, TEXT("TestDriver::LookAtComponentByName - %s.%s aim=%s (loc=%s) rot=%s dist=%.1f"),
		*Actor->GetName(), *ComponentName, *AimPoint.ToString(), *Found->GetComponentLocation().ToString(),
		*NewRot.ToString(), FVector::Dist(AimPoint, CamLoc));
	return true;
}

bool UTestDriverSubsystem::LookAtWorldPoint(const FVector& Point)
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player) { return false; }
	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC) { return false; }
	UCameraComponent* Camera = Player->GetFirstPersonCamera();
	if (!Camera) { return false; }

	const FVector CamLoc = Camera->GetComponentLocation();
	PC->SetControlRotation((Point - CamLoc).GetSafeNormal().Rotation());
	UE_LOG(LogTemp, Log, TEXT("TestDriver::LookAtWorldPoint - aim=%s cam=%s dist=%.1f"),
		*Point.ToString(), *CamLoc.ToString(), FVector::Dist(Point, CamLoc));
	return true;
}

USceneComponent* UTestDriverSubsystem::FindComponentOnActorByName(AActor* Actor, const FString& ComponentName) const
{
	if (!Actor)
	{
		return nullptr;
	}
	TArray<USceneComponent*> SceneComps;
	Actor->GetComponents<USceneComponent>(SceneComps);
	for (USceneComponent* Comp : SceneComps)
	{
		if (Comp && Comp->GetName() == ComponentName)
		{
			return Comp;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("TestDriver::FindComponentOnActorByName - no component '%s' on actor '%s'"),
		*ComponentName, *Actor->GetName());
	return nullptr;
}

bool UTestDriverSubsystem::LookAtSeneca()
{
	ASeneca* Seneca = FindSeneca();
	if (!Seneca)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::LookAtSeneca - no ASeneca in level"));
		return false;
	}
	return LookAt(Seneca);
}

bool UTestDriverSubsystem::LookAtRick()
{
	ARick* Rick = FindRick();
	if (!Rick)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::LookAtRick - no ARick in level"));
		return false;
	}
	return LookAt(Rick);
}

bool UTestDriverSubsystem::LookAtKeyActor()
{
	ASeneca* Seneca = FindSeneca();
	if (!Seneca)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::LookAtKeyActor - no ASeneca in level"));
		return false;
	}
	APropActor* Key = Seneca->GetKeyActor();
	if (!Key)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::LookAtKeyActor - KeyActor not assigned on Seneca"));
		return false;
	}
	return LookAt(Key);
}

AActor* UTestDriverSubsystem::FindActorByLabel(const FString& Label) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
#if WITH_EDITOR
		if (Actor->GetActorLabel() == Label)
		{
			return Actor;
		}
#endif
		if (Actor->GetName() == Label)
		{
			return Actor;
		}
	}

	return nullptr;
}

ASeneca* UTestDriverSubsystem::FindSeneca() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ASeneca> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

ARick* UTestDriverSubsystem::FindRick() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ARick> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

AHudson* UTestDriverSubsystem::FindHudson() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AHudson> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

bool UTestDriverSubsystem::LookAtHudson()
{
	AHudson* Hudson = FindHudson();
	if (!Hudson)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::LookAtHudson - no AHudson in level"));
		return false;
	}
	return LookAt(Hudson);
}

// --- Seneca test helpers ---

void UTestDriverSubsystem::FastForwardSenecaSmoking()
{
	ASeneca* Seneca = FindSeneca();
	if (!Seneca)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::FastForwardSenecaSmoking - no Seneca"));
		return;
	}
	Seneca->FastForwardSmokingAppear();
}

bool UTestDriverSubsystem::HasSenecaAppearedAtSmokingPos() const
{
	ASeneca* Seneca = FindSeneca();
	// OnKeyDropped teleports Seneca to Z=-100000; "appeared" means she's been
	// re-teleported by Tick back to SmokingPositionTarget (above ground).
	return Seneca && Seneca->GetActorLocation().Z > -50000.0;
}

// --- Input simulation ---

void UTestDriverSubsystem::SimulateKeyPress(FKey Key)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SimulateKeyPress - no PlayerController"));
		return;
	}
	PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, EInputEvent::IE_Pressed, /*AmountDepressed=*/1.0f));
	UE_LOG(LogTemp, Log, TEXT("TestDriver::SimulateKeyPress - %s"), *Key.ToString());
}

void UTestDriverSubsystem::SimulateKeyRelease(FKey Key)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SimulateKeyRelease - no PlayerController"));
		return;
	}
	PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, EInputEvent::IE_Released, /*AmountDepressed=*/0.0f));
}

void UTestDriverSubsystem::SimulateMouseX(float Delta)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SimulateMouseX - no PlayerController"));
		return;
	}
	// Use the axis-specific constructor so DeltaTime and NumSamples are populated
	// (CreateSimulated leaves DeltaTime at 0, which breaks axis processing).
	FInputKeyEventArgs Args(
		/*InViewport=*/nullptr,
		INPUTDEVICEID_NONE,
		EKeys::MouseX,
		/*InDelta=*/Delta,
		/*InDeltaTime=*/GetWorld()->GetDeltaSeconds(),
		/*InNumSamples=*/1,
		/*InEventTimestamp=*/FPlatformTime::Cycles64());
	PC->InputKey(Args);
}

// --- Enhanced Input injection ---
//
// APlayerController::InputKey only fires legacy BindAction bindings; Enhanced
// Input actions (HandleInteractTriggered, HandleShowInventory, etc.) need
// their own injection through UEnhancedInputLocalPlayerSubsystem.

void UTestDriverSubsystem::InjectInputAction(UInputAction* Action, bool bPressed)
{
	if (!Action)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InjectInputAction - null action"));
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InjectInputAction - no PlayerController"));
		return;
	}

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InjectInputAction - no LocalPlayer"));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* EIS = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!EIS)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InjectInputAction - no EnhancedInputLocalPlayerSubsystem"));
		return;
	}

	const FInputActionValue Value(bPressed);
	EIS->InjectInputForAction(Action, Value, {}, {});
	UE_LOG(LogTemp, Log, TEXT("TestDriver::InjectInputAction - %s %s"),
		*Action->GetName(), bPressed ? TEXT("PRESSED") : TEXT("RELEASED"));
}

void UTestDriverSubsystem::SimulateInteractPress()
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player) { UE_LOG(LogTemp, Error, TEXT("TestDriver::SimulateInteractPress - no player")); return; }
	InjectInputAction(Player->GetInteractAction(), true);
}

void UTestDriverSubsystem::SimulateInteractRelease()
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player) { return; }
	InjectInputAction(Player->GetInteractAction(), false);
}

void UTestDriverSubsystem::SimulateInventoryPress()
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player) { UE_LOG(LogTemp, Error, TEXT("TestDriver::SimulateInventoryPress - no player")); return; }
	InjectInputAction(Player->GetInventoryAction(), true);
}

void UTestDriverSubsystem::SimulateInventoryRelease()
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player) { return; }
	InjectInputAction(Player->GetInventoryAction(), false);
}

void UTestDriverSubsystem::SimulateSettingsPress()
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player) { UE_LOG(LogTemp, Error, TEXT("TestDriver::SimulateSettingsPress - no player")); return; }
	InjectInputAction(Player->GetSettingsAction(), true);
}

void UTestDriverSubsystem::SimulateSettingsRelease()
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player) { return; }
	InjectInputAction(Player->GetSettingsAction(), false);
}

// --- Sensitivity / look diagnostics ---

void UTestDriverSubsystem::SetGamepadLookSensitivity(float Value)
{
	UWeirdplaceGameUserSettings* Settings = Cast<UWeirdplaceGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SetGamepadLookSensitivity - GameUserSettings is not UWeirdplaceGameUserSettings"));
		return;
	}
	Settings->SetGamepadLookSensitivity(Value);
	UE_LOG(LogTemp, Log, TEXT("TestDriver: GamepadLookSensitivity now %.3f"), Settings->GetGamepadLookSensitivity());
}

void UTestDriverSubsystem::SetMouseLookSensitivity(float Value)
{
	UWeirdplaceGameUserSettings* Settings = Cast<UWeirdplaceGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SetMouseLookSensitivity - GameUserSettings is not UWeirdplaceGameUserSettings"));
		return;
	}
	Settings->SetMouseLookSensitivity(Value);
	UE_LOG(LogTemp, Log, TEXT("TestDriver: MouseLookSensitivity now %.3f"), Settings->GetMouseLookSensitivity());
}

float UTestDriverSubsystem::GetControllerYaw() const
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? static_cast<float>(PC->GetControlRotation().Yaw) : 0.0f;
}

// --- Inventory queries ---

bool UTestDriverSubsystem::IsInventoryFullyOpen() const
{
	UInventoryUIComponent* UI = GetInventoryUIComponent();
	return UI && UI->IsInventoryFullyOpen();
}

bool UTestDriverSubsystem::IsInventoryFullyClosed() const
{
	UInventoryUIComponent* UI = GetInventoryUIComponent();
	return UI && UI->IsInventoryFullyClosed();
}

bool UTestDriverSubsystem::SetSelectedSlot(int32 Index)
{
	UInventoryUIComponent* UI = GetInventoryUIComponent();
	if (!UI)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SetSelectedSlot - no UI component"));
		return false;
	}
	if (!UI->IsInventoryFullyOpen())
	{
		UE_LOG(LogTemp, Warning, TEXT("TestDriver::SetSelectedSlot - inventory not fully open"));
		return false;
	}

	if (!UI->SetSelectedIndexForTest(Index))
	{
		UE_LOG(LogTemp, Warning, TEXT("TestDriver::SetSelectedSlot - index %d out of range"), Index);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("TestDriver::SetSelectedSlot - index %d"), Index);
	return true;
}

// --- Movie helpers ---

AMovieBox* UTestDriverSubsystem::FindNextUncollectedMovie()
{
	// Drop stale weak refs.
	for (auto It = CollectedMovies.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (TActorIterator<AMovieBox> It(GetWorld()); It; ++It)
	{
		if (CollectedMovies.Contains(*It))
		{
			continue;
		}
		// Skip actors without an InteractionText widget (e.g. BP_Spawner1).
		TArray<UWidgetComponent*> Widgets;
		It->GetComponents<UWidgetComponent>(Widgets);
		bool bHasInteractionText = false;
		for (UWidgetComponent* W : Widgets)
		{
			if (W->GetFName() == TEXT("InteractionText"))
			{
				bHasInteractionText = true;
				break;
			}
		}
		if (!bHasInteractionText)
		{
			continue;
		}
		LastFoundMovie = *It;
		return *It;
	}

	UE_LOG(LogTemp, Error, TEXT("TestDriver::FindNextUncollectedMovie - no uncollected MovieBox in level"));
	return nullptr;
}

AMovieBox* UTestDriverSubsystem::FindBlankTape() const
{
	for (TActorIterator<AMovieBox> It(GetWorld()); It; ++It)
	{
		AMovieBox* Box = *It;
		if (!Box)
		{
			continue;
		}
		const AMovieBox* CDO = Box->GetClass()->GetDefaultObject<AMovieBox>();
		if (CDO && CDO->bExemptFromMovieLimit)
		{
			return Box;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("TestDriver::FindBlankTape - no blank tape found in level"));
	return nullptr;
}

void UTestDriverSubsystem::MarkLastFoundMovieCollected()
{
	if (LastFoundMovie.IsValid())
	{
		CollectedMovies.Add(LastFoundMovie.Get());
		UE_LOG(LogTemp, Log, TEXT("TestDriver::MarkLastFoundMovieCollected - %s"), *LastFoundMovie->GetName());
		LastFoundMovie.Reset();
	}
}

bool UTestDriverSubsystem::TriggerCollectInspectedMovie()
{
	for (TActorIterator<AMovieBox> It(GetWorld()); It; ++It)
	{
		AMovieBox* Box = *It;
		if (Box && Box->IsBeingInspected())
		{
			UE_LOG(LogTemp, Log, TEXT("TestDriver::TriggerCollectInspectedMovie - %s"), *Box->GetName());
			Box->CollectInspectedMovie();
			return true;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("TestDriver::TriggerCollectInspectedMovie - no MovieBox in inspection"));
	return false;
}

// --- State queries ---

EPlayerActivityState UTestDriverSubsystem::GetActivityState() const
{
	AFirstPersonCharacter* Player = GetPlayer();
	return Player ? Player->GetActivityState() : EPlayerActivityState::FreeRoaming;
}

bool UTestDriverSubsystem::IsInSimpleDialogue() const
{
	return GetActivityState() == EPlayerActivityState::InSimpleDialogue;
}

bool UTestDriverSubsystem::IsInAnyDialogue() const
{
	AFirstPersonCharacter* Player = GetPlayer();
	return Player && Player->IsInAnyDialogue();
}

bool UTestDriverSubsystem::HasItem(FName ItemId) const
{
	UInventoryComponent* Inv = GetInventoryComponent();
	return Inv && Inv->HasItem(ItemId);
}

int32 UTestDriverSubsystem::GetInventoryCount() const
{
	UInventoryComponent* Inv = GetInventoryComponent();
	return Inv ? Inv->GetItemCount() : 0;
}

namespace
{
	// The reusable UGazeRewardComponent can now live on many actors (the user
	// adds it wherever). The gas-station E2E tests specifically want the canopy
	// light's component — find it by the 'GazeRewardTarget' tag, not "first one".
	UGazeRewardComponent* FindGasStationGazeComponent(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(FName("GazeRewardTarget")))
			{
				if (UGazeRewardComponent* Gaze = It->FindComponentByClass<UGazeRewardComponent>())
				{
					return Gaze;
				}
			}
		}
		return nullptr;
	}
}

bool UTestDriverSubsystem::GetGazeHumState(float& OutVolume, bool& bOutPlaying) const
{
	UGazeRewardComponent* Gaze = FindGasStationGazeComponent(GetWorld());
	if (!Gaze)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetGazeHumState - no UGazeRewardComponent on a 'GazeRewardTarget' actor"));
		return false;
	}
	UAudioComponent* Audio = Gaze->GetHumComponent();
	if (!Audio)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetGazeHumState - GazeReward component has no hum"));
		return false;
	}
	OutVolume = Audio->VolumeMultiplier;
	bOutPlaying = Audio->IsPlaying();
	return true;
}

bool UTestDriverSubsystem::GetGazeRewardSeconds(float& OutSeconds) const
{
	UGazeRewardComponent* Gaze = FindGasStationGazeComponent(GetWorld());
	if (!Gaze)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetGazeRewardSeconds - no UGazeRewardComponent on a 'GazeRewardTarget' actor"));
		return false;
	}
	OutSeconds = Gaze->GetGazeSeconds();
	return true;
}

bool UTestDriverSubsystem::GetGazeEffectWeight(float& OutWeight) const
{
	UGazeRewardComponent* Gaze = FindGasStationGazeComponent(GetWorld());
	if (!Gaze)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetGazeEffectWeight - no UGazeRewardComponent on a 'GazeRewardTarget' actor"));
		return false;
	}
	OutWeight = Gaze->GetCurrentEffectWeight();
	return true;
}

bool UTestDriverSubsystem::GetBlankVhsGazeState(bool& bOutHasChosen, bool& bOutLooking, bool& bOutHadHit,
	FString& OutHitActor, FString& OutHitComponent, float& OutHitDistance,
	FVector& OutImpactPoint, float& OutVolume) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (USpawnerActorComponent* Spawner = It->FindComponentByClass<USpawnerActorComponent>())
		{
			Spawner->GetGazeDebugState(bOutHasChosen, bOutLooking, bOutHadHit,
				OutHitActor, OutHitComponent, OutHitDistance, OutImpactPoint, OutVolume);
			return true;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("TestDriver::GetBlankVhsGazeState - no USpawnerActorComponent in level"));
	return false;
}

bool UTestDriverSubsystem::GetDisplayedDialogue(FString& OutSpeaker, FString& OutBody) const
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetDisplayedDialogue - no player"));
		return false;
	}

	UUI_Dialogue* Widget = Player->GetActiveDialogueWidget();
	if (!Widget)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetDisplayedDialogue - no active dialogue widget"));
		return false;
	}

	OutSpeaker = Widget->GetDisplayedSpeaker();
	OutBody = Widget->GetFullLineText();
	return true;
}

bool UTestDriverSubsystem::GetPutBackPromptState(FString& OutText, float& OutFacingDot, bool& bOutVisible) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AMovieBox* Inspected = nullptr;
	for (TActorIterator<AMovieBox> It(World); It; ++It)
	{
		if (It->IsBeingInspected())
		{
			Inspected = *It;
			break;
		}
	}
	if (!Inspected)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetPutBackPromptState - no MovieBox is being inspected"));
		return false;
	}

	UTextRenderComponent* Prompt = nullptr;
	TArray<UTextRenderComponent*> Texts;
	Inspected->GetComponents<UTextRenderComponent>(Texts);
	for (UTextRenderComponent* T : Texts)
	{
		if (T->GetFName() == TEXT("PutBackPromptText"))
		{
			Prompt = T;
			break;
		}
	}
	if (!Prompt)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetPutBackPromptState - inspected MovieBox '%s' has no 'PutBackPromptText' component"),
			*Inspected->GetName());
		return false;
	}

	OutText = Prompt->Text.ToString();
	bOutVisible = Prompt->IsVisible();

	FVector CamLoc;
	FRotator CamRot;
	World->GetFirstPlayerController()->GetPlayerViewPoint(CamLoc, CamRot);
	// The prompt is yaw-only billboarded (stays upright), so measure facing in
	// the horizontal plane — a pitch difference to the camera is expected and
	// shouldn't count against it.
	FVector ToCam = CamLoc - Prompt->GetComponentLocation();
	FVector Forward = Prompt->GetForwardVector();
	ToCam.Z = 0.f;
	Forward.Z = 0.f;
	OutFacingDot = FVector::DotProduct(Forward.GetSafeNormal(), ToCam.GetSafeNormal());
	return true;
}

bool UTestDriverSubsystem::GetHeldItemBoxAxes(FVector& OutLongAxisCamSpace, FVector& OutShortAxisCamSpace, float& OutMaxExtent, FVector& OutCenterCamSpace) const
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetHeldItemBoxAxes - no player"));
		return false;
	}

	// The pawn can carry more than one UHeldItemComponent (C++ default
	// subobject + BP-added); only the live one owns a held mesh. Use that one.
	UStaticMeshComponent* Mesh = nullptr;
	TArray<UHeldItemComponent*> HeldComps;
	Player->GetComponents<UHeldItemComponent>(HeldComps);
	for (UHeldItemComponent* Comp : HeldComps)
	{
		if (Comp->GetHeldItemMeshComponent())
		{
			Mesh = Comp->GetHeldItemMeshComponent();
			break;
		}
	}
	if (!Mesh || !Mesh->GetStaticMesh() || !Mesh->IsVisible())
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetHeldItemBoxAxes - no visible held item mesh (comps=%d mesh=%d staticmesh=%d isvisible=%d)"),
			HeldComps.Num(),
			Mesh != nullptr,
			Mesh && Mesh->GetStaticMesh() != nullptr,
			Mesh && Mesh->IsVisible());
		return false;
	}

	// Identify the mesh's longest and shortest local axes after scale. When
	// two scaled extents are within 20% (the blank tape's box is nearly
	// square at gameplay scale), fall back to the authored extents, which
	// are unambiguous.
	const FVector Authored = Mesh->GetStaticMesh()->GetBoundingBox().GetExtent();
	const FVector Ext = Authored * Mesh->GetComponentScale().GetAbs();
	int32 LongIdx = 0, ShortIdx = 0;
	for (int32 i = 1; i < 3; ++i)
	{
		if (Ext[i] > Ext[LongIdx]) { LongIdx = i; }
		if (Ext[i] < Ext[ShortIdx]) { ShortIdx = i; }
	}
	for (int32 i = 0; i < 3; ++i)
	{
		if (i != LongIdx && i != ShortIdx
			&& Ext[i] > Ext[LongIdx] * 0.8f
			&& Authored[i] > Authored[LongIdx])
		{
			LongIdx = i;
		}
	}
	OutMaxExtent = Ext[LongIdx];
	FVector LocalLong = FVector::ZeroVector;
	FVector LocalShort = FVector::ZeroVector;
	LocalLong[LongIdx] = 1.f;
	LocalShort[ShortIdx] = 1.f;

	FVector CamLoc;
	FRotator CamRot;
	GetWorld()->GetFirstPlayerController()->GetPlayerViewPoint(CamLoc, CamRot);
	const FQuat CamQ = CamRot.Quaternion();
	const FQuat MeshQ = Mesh->GetComponentQuat();
	OutLongAxisCamSpace = CamQ.UnrotateVector(MeshQ.RotateVector(LocalLong));
	OutShortAxisCamSpace = CamQ.UnrotateVector(MeshQ.RotateVector(LocalShort));

	const FVector WorldCenter = Mesh->GetComponentTransform().TransformPosition(
		Mesh->GetStaticMesh()->GetBoundingBox().GetCenter());
	OutCenterCamSpace = CamQ.UnrotateVector(WorldCenter - CamLoc);

	// Full local->camera mapping, for diagnosing pose-correction values.
	const FQuat MeshToCam = CamQ.Inverse() * MeshQ;
	UE_LOG(LogTemp, Log, TEXT("TestDriver::GetHeldItemBoxAxes - ext=%s X->%s Y->%s Z->%s relrot=%s"),
		*Ext.ToString(),
		*MeshToCam.RotateVector(FVector::XAxisVector).ToString(),
		*MeshToCam.RotateVector(FVector::YAxisVector).ToString(),
		*MeshToCam.RotateVector(FVector::ZAxisVector).ToString(),
		*Mesh->GetRelativeRotation().ToString());
	return true;
}

bool UTestDriverSubsystem::ActivateBlankTapeForTest()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (USpawnerActorComponent* Spawner = It->FindComponentByClass<USpawnerActorComponent>())
		{
			Spawner->ActivateChosenTape();
			return true;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("TestDriver::ActivateBlankTapeForTest - no USpawnerActorComponent in level"));
	return false;
}

bool UTestDriverSubsystem::CollectBlankTapeForTest()
{
	AMovieBox* Tape = FindBlankTape();
	if (!Tape)
	{
		return false;
	}
	// CollectInspectedMovie runs the real capture (AddItemToInventoryWithMesh
	// off the envelope) and its deferred StopInspection no-ops outside
	// inspection.
	Tape->CollectInspectedMovie();
	return Tape->WasCollected();
}

bool UTestDriverSubsystem::AddTestItem(FName ItemId, const FString& MeshAssetPath, FVector Scale)
{
	UInventoryComponent* Inv = GetInventoryComponent();
	if (!Inv)
	{
		return false;
	}
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshAssetPath);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTestItem: failed to load mesh '%s'"), *MeshAssetPath);
		return false;
	}
	FInventoryItemData Data;
	Data.ItemID = ItemId;
	Data.Mesh = Mesh;
	Data.Scale = Scale;
	for (int32 i = 0; i < Mesh->GetStaticMaterials().Num(); ++i)
	{
		Data.Materials.Add(Mesh->GetMaterial(i));
	}
	Inv->AddItemWithData(Data);
	return true;
}

int32 UTestDriverSubsystem::AddAllItemDefsFromFolder(const FString& FolderPath)
{
#if WITH_EDITOR
	UInventoryComponent* Inv = GetInventoryComponent();
	if (!Inv)
	{
		UE_LOG(LogTemp, Error, TEXT("AddAllItemDefsFromFolder: no inventory component"));
		return 0;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> AssetDatas;
	AssetRegistry.GetAssetsByPath(FName(*FolderPath), AssetDatas, /*bRecursive*/ true);

	int32 Added = 0;
	for (const FAssetData& AD : AssetDatas)
	{
		UItemDefinition* Def = Cast<UItemDefinition>(AD.GetAsset());
		if (!Def || Def->ItemID.IsNone() || !Def->Mesh)
		{
			continue;
		}
		Inv->AddItemWithData(Def->ToInventoryItemData());
		UE_LOG(LogTemp, Log, TEXT("AddAllItemDefsFromFolder: added '%s'"), *Def->ItemID.ToString());
		++Added;
	}
	return Added;
#else
	return 0;
#endif
}

void UTestDriverSubsystem::SetTestStatus(const FString& Step)
{
	UE_LOG(LogTemp, Log, TEXT("TestDriver::Status - %s"), *Step);
	if (GEngine)
	{
		// Fixed key so each call replaces the previous line. Large lifetime
		// keeps it pinned until the next update or end of test.
		GEngine->AddOnScreenDebugMessage(
			/*Key*/ 987654,
			/*TimeToDisplay*/ 9999.f,
			FColor::Yellow,
			FString::Printf(TEXT("[E2E] %s"), *Step),
			/*bNewerOnTop*/ false,
			/*TextScale*/ FVector2D(1.4f, 1.4f));
	}
}

UInventoryComponent* UTestDriverSubsystem::GetInventoryComponent() const
{
	AFirstPersonCharacter* Player = GetPlayer();
	return Player ? Player->GetInventoryComponent() : nullptr;
}

UInventoryUIComponent* UTestDriverSubsystem::GetInventoryUIComponent() const
{
	AFirstPersonCharacter* Player = GetPlayer();
	return Player ? Player->GetInventoryUIComponent() : nullptr;
}
