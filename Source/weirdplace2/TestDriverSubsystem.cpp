#include "TestDriverSubsystem.h"
#include "FirstPersonCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "MovieBox.h"
#include "PropActor.h"
#include "Hudson.h"
#include "Rick.h"
#include "Seneca.h"
#include "TestWaypoint.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
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

	USceneComponent* Found = nullptr;
	TArray<USceneComponent*> SceneComps;
	Actor->GetComponents<USceneComponent>(SceneComps);
	for (USceneComponent* Comp : SceneComps)
	{
		if (Comp && Comp->GetName() == ComponentName)
		{
			Found = Comp;
			break;
		}
	}
	if (!Found)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::LookAtActorComponentByName - no component '%s' on actor '%s'"),
			*ComponentName, *ActorLabel);
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

	UE_LOG(LogTemp, Log, TEXT("TestDriver::LookAtActorComponentByName - %s.%s aim=%s (loc=%s) rot=%s dist=%.1f"),
		*ActorLabel, *ComponentName, *AimPoint.ToString(), *Found->GetComponentLocation().ToString(),
		*NewRot.ToString(), FVector::Dist(AimPoint, CamLoc));
	return true;
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
	PC->InputKey(FInputKeyParams(Key, EInputEvent::IE_Pressed, FVector::ZeroVector));
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
	PC->InputKey(FInputKeyParams(Key, EInputEvent::IE_Released, FVector::ZeroVector));
}

void UTestDriverSubsystem::SimulateMouseX(float Delta)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SimulateMouseX - no PlayerController"));
		return;
	}
	// Axis key: use the constructor that takes (Key, Delta, DeltaTime, NumSamples).
	PC->InputKey(FInputKeyParams(EKeys::MouseX, (double)Delta, GetWorld()->GetDeltaSeconds(), 1));
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

void UTestDriverSubsystem::MarkLastFoundMovieCollected()
{
	if (LastFoundMovie.IsValid())
	{
		CollectedMovies.Add(LastFoundMovie.Get());
		UE_LOG(LogTemp, Log, TEXT("TestDriver::MarkLastFoundMovieCollected - %s"), *LastFoundMovie->GetName());
		LastFoundMovie.Reset();
	}
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
