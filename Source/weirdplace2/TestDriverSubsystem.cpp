#include "TestDriverSubsystem.h"
#include "BladderUrgencyComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "KeypadUIComponent.h"
#include "ItemDefinition.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#endif
#include "MovieBox.h"
#include "InspectablePickup.h"
#include "OutsideBathroomDoor.h"
#include "PropActor.h"
#include "SpawnerActorComponent.h"
#include "GazeRewardComponent.h"
#include "StorySubsystem.h"
#include "CRTTV.h"
#include "StormBeatController.h"
#include "PayPhone.h"
#include "Components/LightComponent.h"
#include "Sound/AmbientSound.h"
#include "Hudson.h"
#include "Rick.h"
#include "Seneca.h"
#include "TestWaypoint.h"
#include "UI_Dialogue.h"
#include "DialogueWidgetProvider.h"
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

// Returns the first actor of type T in the world, or nullptr. Collapses the
// otherwise-identical singleton finders (Seneca/Rick/Hudson/PayPhone/pickup).
template<typename T>
static T* FindFirstActor(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<T> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

AFirstPersonCharacter* UTestDriverSubsystem::GetPlayer() const
{
	return Cast<AFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
}

void UTestDriverSubsystem::SetStoryFlag(FName FlagName, bool bValue)
{
	UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr;
	if (!Story)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SetStoryFlag - no UStorySubsystem"));
		return;
	}
	EStoryFlag Flag;
	if (!UStorySubsystem::TryParseStoryFlag(FlagName, Flag))
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SetStoryFlag - unknown flag '%s'"), *FlagName.ToString());
		return;
	}
	Story->SetFlag(Flag, bValue);
}

bool UTestDriverSubsystem::IsStoryFlagSet(FName FlagName) const
{
	UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr;
	if (!Story)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::IsStoryFlagSet - no UStorySubsystem"));
		return false;
	}
	EStoryFlag Flag;
	if (!UStorySubsystem::TryParseStoryFlag(FlagName, Flag))
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::IsStoryFlagSet - unknown flag '%s'"), *FlagName.ToString());
		return false;
	}
	return Story->IsFlagSet(Flag);
}

void UTestDriverSubsystem::TriggerStoreEntry()
{
	UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr;
	if (!Story)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::TriggerStoreEntry - no UStorySubsystem"));
		return;
	}
	Story->HandleStoreEntry();
}

bool UTestDriverSubsystem::IsTvShowingWarning(const FString& Label) const
{
	ACRTTV* TV = Cast<ACRTTV>(FindActorByLabel(Label));
	if (!TV)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::IsTvShowingWarning - no ACRTTV labeled '%s'"), *Label);
		return false;
	}
	return TV->IsShowingWarning();
}

bool UTestDriverSubsystem::IsTvWarningAudioPlaying(const FString& Label) const
{
	ACRTTV* TV = Cast<ACRTTV>(FindActorByLabel(Label));
	if (!TV)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::IsTvWarningAudioPlaying - no ACRTTV labeled '%s'"), *Label);
		return false;
	}
	return TV->IsWarningAudioPlaying();
}

bool UTestDriverSubsystem::IsAmbientSoundPlaying(const FString& Label) const
{
	AAmbientSound* Ambient = Cast<AAmbientSound>(FindActorByLabel(Label));
	if (!Ambient)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::IsAmbientSoundPlaying - no AAmbientSound labeled '%s'"), *Label);
		return false;
	}
	UAudioComponent* AudioComp = Ambient->GetAudioComponent();
	return AudioComp && AudioComp->IsPlaying();
}

float UTestDriverSubsystem::GetActorMaxLightIntensity(const FString& Label) const
{
	AActor* Actor = FindActorByLabel(Label);
	if (!Actor)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetActorMaxLightIntensity - no actor '%s'"), *Label);
		return -1.f;
	}
	TArray<ULightComponent*> LightComps;
	Actor->GetComponents<ULightComponent>(LightComps);
	if (LightComps.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetActorMaxLightIntensity - '%s' has no ULightComponent"), *Label);
		return -1.f;
	}
	float MaxIntensity = 0.f;
	for (const ULightComponent* Light : LightComps)
	{
		MaxIntensity = FMath::Max(MaxIntensity, Light->Intensity);
	}
	return MaxIntensity;
}

void UTestDriverSubsystem::SpawnAndConfigureStormBeat(const TArray<FString>& LightLabels, const TArray<FString>& HideLabels,
	const TArray<FString>& AmbientLabels, float Multiplier)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SpawnAndConfigureStormBeat - no world"));
		return;
	}

	// Isolate the test: destroy any designer-placed AStormBeatController in the PIE
	// copy so only this test's controller (known lights/mult) reacts to the flag.
	// Otherwise a placed controller's own multiplier/targets contaminate the asserts.
	int32 Removed = 0;
	for (TActorIterator<AStormBeatController> It(World); It; ++It)
	{
		It->Destroy();
		++Removed;
	}
	if (Removed > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("TestDriver::SpawnAndConfigureStormBeat - removed %d pre-placed controller(s) for test isolation"), Removed);
	}

	TArray<AActor*> Lights;
	for (const FString& Label : LightLabels)
	{
		if (AActor* Light = FindActorByLabel(Label))
		{
			Lights.Add(Light);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("TestDriver::SpawnAndConfigureStormBeat - no light actor '%s'"), *Label);
		}
	}

	TArray<AActor*> Hide;
	for (const FString& Label : HideLabels)
	{
		if (AActor* Actor = FindActorByLabel(Label))
		{
			Hide.Add(Actor);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("TestDriver::SpawnAndConfigureStormBeat - no hide actor '%s'"), *Label);
		}
	}

	TArray<AAmbientSound*> Ambients;
	for (const FString& Label : AmbientLabels)
	{
		if (AAmbientSound* Ambient = Cast<AAmbientSound>(FindActorByLabel(Label)))
		{
			Ambients.Add(Ambient);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("TestDriver::SpawnAndConfigureStormBeat - no AAmbientSound '%s'"), *Label);
		}
	}

	// Spawn deferred so it's configured BEFORE BeginPlay subscribes to the flag.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStormBeatController* Controller = World->SpawnActorDeferred<AStormBeatController>(
		AStormBeatController::StaticClass(), FTransform::Identity);
	if (!Controller)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SpawnAndConfigureStormBeat - spawn failed"));
		return;
	}
	Controller->ConfigureForTest(Lights, Hide, Ambients, Multiplier);
	Controller->FinishSpawning(FTransform::Identity);
	UE_LOG(LogTemp, Log, TEXT("TestDriver::SpawnAndConfigureStormBeat - spawned controller with %d light(s), %d hide, %d ambient(s), mult %.2f"),
		Lights.Num(), Hide.Num(), Ambients.Num(), Multiplier);
}

bool UTestDriverSubsystem::IsActorVisibleByLabel(const FString& Label) const
{
	AActor* Actor = FindActorByLabel(Label);
	if (!Actor)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::IsActorVisibleByLabel - no actor '%s'"), *Label);
		return false;
	}
	USceneComponent* Root = Actor->GetRootComponent();
	if (!Root)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::IsActorVisibleByLabel - '%s' has no root"), *Label);
		return false;
	}
	return Root->IsVisible();
}

bool UTestDriverSubsystem::IsPayPhoneAudioPlaying() const
{
	APayPhone* Phone = FindFirstActor<APayPhone>(GetWorld());
	if (!Phone)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::IsPayPhoneAudioPlaying - no APayPhone in level"));
		return false;
	}
	return Phone->IsAudioPlaying();
}

bool UTestDriverSubsystem::CanPayPhoneInteract() const
{
	APayPhone* Phone = FindFirstActor<APayPhone>(GetWorld());
	if (!Phone)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::CanPayPhoneInteract - no APayPhone in level"));
		return false;
	}
	return Phone->CanInteract();
}

bool UTestDriverSubsystem::IsPayPhoneDialtonePlaying() const
{
	APayPhone* Phone = FindFirstActor<APayPhone>(GetWorld());
	if (!Phone)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::IsPayPhoneDialtonePlaying - no APayPhone in level"));
		return false;
	}
	return Phone->IsDialtonePlaying();
}

void UTestDriverSubsystem::TriggerPayPhonePickup()
{
	APayPhone* Phone = FindFirstActor<APayPhone>(GetWorld());
	if (!Phone)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::TriggerPayPhonePickup - no APayPhone in level"));
		return;
	}
	Phone->Interact_Implementation();
}

void UTestDriverSubsystem::TriggerPayPhoneHangUp()
{
	APayPhone* Phone = FindFirstActor<APayPhone>(GetWorld());
	if (!Phone)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::TriggerPayPhoneHangUp - no APayPhone in level"));
		return;
	}
	Phone->HangUp();
}

void UTestDriverSubsystem::MarkPayPhoneCodeSpoken()
{
	APayPhone* Phone = FindFirstActor<APayPhone>(GetWorld());
	if (!Phone)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::MarkPayPhoneCodeSpoken - no APayPhone in level"));
		return;
	}
	Phone->MarkCodeSpokenForTest();
}

int32 UTestDriverSubsystem::GetBathroomDoorLockedSoundCount() const
{
	for (TActorIterator<AOutsideBathroomDoor> It(GetWorld()); It; ++It)
	{
		return It->GetLockedSoundPlayCount();
	}
	UE_LOG(LogTemp, Error, TEXT("TestDriver::GetBathroomDoorLockedSoundCount - no AOutsideBathroomDoor in level"));
	return -1;
}

bool UTestDriverSubsystem::GetBathroomDoorAnimKeyGlowActive() const
{
	for (TActorIterator<AOutsideBathroomDoor> It(GetWorld()); It; ++It)
	{
		return It->IsAnimKeyGlowActive();
	}
	UE_LOG(LogTemp, Error, TEXT("TestDriver::GetBathroomDoorAnimKeyGlowActive - no AOutsideBathroomDoor in level"));
	return false;
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

bool UTestDriverSubsystem::TriggerBladderPulse()
{
	AFirstPersonCharacter* Player = GetPlayer();
	UBladderUrgencyComponent* Bladder = Player ? Player->FindComponentByClass<UBladderUrgencyComponent>() : nullptr;
	if (!Bladder)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::TriggerBladderPulse - no bladder component"));
		return false;
	}
	Bladder->FireSinglePulse();
	return true;
}

bool UTestDriverSubsystem::SetHeightFogVisible(bool bVisible)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		if (UExponentialHeightFogComponent* Fog = It->GetComponent())
		{
			Fog->SetVisibility(bVisible);
			UE_LOG(LogTemp, Log, TEXT("TestDriver::SetHeightFogVisible - %d"), bVisible ? 1 : 0);
			return true;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("TestDriver::SetHeightFogVisible - no ExponentialHeightFog in world"));
	return false;
}

bool UTestDriverSubsystem::GetNamedComponentVisible(const FString& ActorLabel, const FString& ComponentName, bool& bOutVisible) const
{
	AActor* Actor = FindActorByLabel(ActorLabel);
	if (!Actor)
	{
		return false;
	}
	USceneComponent* Component = FindComponentOnActorByName(Actor, ComponentName);
	if (!Component)
	{
		return false;
	}
	bOutVisible = Component->IsVisible();
	return true;
}

bool UTestDriverSubsystem::GetCameraDofState(bool& bOutOverrideActive, float& OutFstop, float& OutFocalDistance) const
{
	AFirstPersonCharacter* Player = GetPlayer();
	UCameraComponent* Camera = Player ? Player->GetFirstPersonCamera() : nullptr;
	if (!Camera)
	{
		return false;
	}
	const FPostProcessSettings& PP = Camera->PostProcessSettings;
	bOutOverrideActive = PP.bOverride_DepthOfFieldFstop && PP.bOverride_DepthOfFieldFocalDistance;
	OutFstop = PP.DepthOfFieldFstop;
	OutFocalDistance = PP.DepthOfFieldFocalDistance;
	return true;
}

bool UTestDriverSubsystem::GetDialogueBackingState(const FString& ActorLabel, bool& bOutHasBacking, bool& bOutDialogueOpen) const
{
	AActor* Actor = FindActorByLabel(ActorLabel);
	IDialogueWidgetProvider* Provider = Cast<IDialogueWidgetProvider>(Actor);
	if (!Provider)
	{
		return false;
	}
	UUI_Dialogue* Widget = Provider->GetDialogueWidget();
	if (!Widget)
	{
		return false;
	}
	bOutHasBacking = Widget->HasTextBacking();
	bOutDialogueOpen = Widget->IsDialogueOpen();
	return true;
}

bool UTestDriverSubsystem::TeleportToWorldPoint(const FVector& GroundPoint)
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::TeleportToWorldPoint - no player"));
		return false;
	}
	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector NewLoc = GroundPoint + FVector(0.f, 0.f, HalfHeight);
	Player->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogTemp, Log, TEXT("TestDriver::TeleportToWorldPoint - %s"), *NewLoc.ToString());
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

bool UTestDriverSubsystem::LookAtTelephone()
{
	APayPhone* PayPhone = FindPayPhone();
	if (!PayPhone)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::LookAtTelephone - no APayPhone in level"));
		return false;
	}
	return LookAt(PayPhone);
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
	return FindFirstActor<ASeneca>(GetWorld());
}

APayPhone* UTestDriverSubsystem::FindPayPhone() const
{
	return FindFirstActor<APayPhone>(GetWorld());
}

ARick* UTestDriverSubsystem::FindRick() const
{
	return FindFirstActor<ARick>(GetWorld());
}

AHudson* UTestDriverSubsystem::FindHudson() const
{
	return FindFirstActor<AHudson>(GetWorld());
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

bool UTestDriverSubsystem::GetSenecaSmokingLinesJoined(FString& Out) const
{
	ASeneca* Seneca = FindSeneca();
	if (!Seneca)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetSenecaSmokingLinesJoined - no Seneca"));
		return false;
	}
	TArray<FText> Lines;
	Seneca->BuildEffectiveDialogueLines(ESenecaState::Smoking, Lines);
	TArray<FString> Strs;
	for (const FText& L : Lines)
	{
		Strs.Add(L.ToString());
	}
	Out = FString::Join(Strs, TEXT(" "));
	UE_LOG(LogTemp, Log, TEXT("TestDriver::GetSenecaSmokingLinesJoined: '%s'"), *Out);
	return true;
}

void UTestDriverSubsystem::ForceSenecaToSmoking()
{
	ASeneca* Seneca = FindSeneca();
	if (!Seneca)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::ForceSenecaToSmoking - no Seneca"));
		return;
	}
	Seneca->CurrentState = ESenecaState::Smoking;
	Seneca->ForceSmokingAppearance();
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

void UTestDriverSubsystem::SimulatePutBack()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SimulatePutBack - no PlayerController"));
		return;
	}

	// "Put back" exits item inspection via the legacy "Exit Interaction"
	// BindAction (see AMovieBox::SetupPlayerInputComponent), so it fires through
	// APlayerController::InputKey — not Enhanced Input injection. Press the key
	// for the player's current input device: keyboard Q or gamepad B.
	AFirstPersonCharacter* Player = GetPlayer();
	const bool bGamepad = Player && Player->IsUsingGamepad();
	const FKey Key = bGamepad ? EKeys::Gamepad_FaceButton_Right : EKeys::Q;
	PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, EInputEvent::IE_Pressed, /*AmountDepressed=*/1.0f));
	UE_LOG(LogTemp, Log, TEXT("TestDriver::SimulatePutBack - %s (%s)"),
		*Key.ToString(), bGamepad ? TEXT("gamepad") : TEXT("keyboard"));
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

// --- Inspectable pickup helpers ---

AInspectablePickup* UTestDriverSubsystem::FindInspectablePickup() const
{
	return FindFirstActor<AInspectablePickup>(GetWorld());
}

bool UTestDriverSubsystem::TriggerCollectInspectedPickup()
{
	for (TActorIterator<AInspectablePickup> It(GetWorld()); It; ++It)
	{
		AInspectablePickup* Pickup = *It;
		if (Pickup && Pickup->IsBeingInspected())
		{
			UE_LOG(LogTemp, Log, TEXT("TestDriver::TriggerCollectInspectedPickup - %s"), *Pickup->GetName());
			Pickup->CollectInspectedItem();
			return true;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("TestDriver::TriggerCollectInspectedPickup - no InspectablePickup in inspection"));
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

FName UTestDriverSubsystem::GetInventoryItemAt(int32 SlotIndex) const
{
	UInventoryComponent* Inv = GetInventoryComponent();
	if (!Inv)
	{
		return NAME_None;
	}
	const TArray<FName> Items = Inv->GetItems();
	return Items.IsValidIndex(SlotIndex) ? Items[SlotIndex] : NAME_None;
}

bool UTestDriverSubsystem::GetMoviePosterState(int32 PosterIndex, bool& bOutVisible, FString& OutMaterialName) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FName Tag(*FString::Printf(TEXT("MoviePoster%d"), PosterIndex));
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->ActorHasTag(Tag))
		{
			continue;
		}

		TArray<UStaticMeshComponent*> Meshes;
		It->GetComponents(Meshes);
		UStaticMeshComponent* Surface = nullptr;
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetName() == TEXT("PosterSheet"))
			{
				Surface = Mesh;
				break;
			}
		}
		if (!Surface && Meshes.Num() == 1)
		{
			Surface = Meshes[0];
		}
		if (!Surface)
		{
			return false;
		}

		bOutVisible = Surface->IsVisible();
		UMaterialInterface* Mat = Surface->GetMaterial(0);
		OutMaterialName = Mat ? Mat->GetName() : FString();
		// Poster MIDs carry the cover in their CoverTexture param — report the
		// texture name (== ItemID) as the poster's identity when present.
		UTexture* CoverTex = nullptr;
		if (Mat && Mat->GetTextureParameterValue(FMaterialParameterInfo(FName("CoverTexture")), CoverTex) && CoverTex)
		{
			OutMaterialName = CoverTex->GetName();
		}
		return true;
	}
	return false;
}

int32 UTestDriverSubsystem::GetSelectedSlot() const
{
	UInventoryUIComponent* UI = GetInventoryUIComponent();
	return UI ? UI->GetSelectedIndex() : -1;
}

int32 UTestDriverSubsystem::GetScrollOffset() const
{
	UInventoryUIComponent* UI = GetInventoryUIComponent();
	return UI ? UI->GetScrollOffsetForTest() : -1;
}

int32 UTestDriverSubsystem::GetVisibleColumns() const
{
	UInventoryUIComponent* UI = GetInventoryUIComponent();
	return UI ? UI->GetVisibleColumnsForTest() : 0;
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

bool UTestDriverSubsystem::GetGazeCameraFOV(float& OutFOV) const
{
	UGazeRewardComponent* Gaze = FindGasStationGazeComponent(GetWorld());
	if (!Gaze)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetGazeCameraFOV - no UGazeRewardComponent on a 'GazeRewardTarget' actor"));
		return false;
	}
	OutFOV = Gaze->GetCurrentFOV();
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

bool UTestDriverSubsystem::GetInspectablePickupGlowActive() const
{
	AInspectablePickup* Pickup = FindInspectablePickup();
	if (!Pickup)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::GetInspectablePickupGlowActive - no AInspectablePickup in level"));
		return false;
	}
	return Pickup->IsGlowActive();
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

UKeypadUIComponent* UTestDriverSubsystem::GetKeypadUIComponent() const
{
	AFirstPersonCharacter* Player = GetPlayer();
	return Player ? Player->GetKeypadUIComponent() : nullptr;
}

bool UTestDriverSubsystem::IsKeypadFullyOpen() const
{
	UKeypadUIComponent* KP = GetKeypadUIComponent();
	return KP && KP->IsKeypadFullyOpen();
}

int32 UTestDriverSubsystem::GetKeypadDenySoundCount() const
{
	UKeypadUIComponent* KP = GetKeypadUIComponent();
	return KP ? KP->GetDenySoundPlayCount() : -1;
}

bool UTestDriverSubsystem::EnterKeypadCode(const FString& Code)
{
	UKeypadUIComponent* KP = GetKeypadUIComponent();
	if (!KP)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::EnterKeypadCode - no keypad component"));
		return false;
	}
	if (!KP->IsKeypadFullyOpen())
	{
		UE_LOG(LogTemp, Warning, TEXT("TestDriver::EnterKeypadCode - keypad not fully open"));
		return false;
	}

	for (int32 i = 0; i < Code.Len(); i++)
	{
		const int32 Digit = static_cast<int32>(Code[i]) - static_cast<int32>('0');
		if (Digit < 1 || Digit > 9)
		{
			UE_LOG(LogTemp, Error, TEXT("TestDriver::EnterKeypadCode - '%c' is not a digit 1-9"), Code[i]);
			return false;
		}
		// Cell index = digit - 1 (cell 0 == "1"). The last press submits + closes.
		if (!KP->SetSelectedDigitForTest(Digit - 1))
		{
			return false;
		}
		KP->PressSelectedDigit();
	}
	UE_LOG(LogTemp, Log, TEXT("TestDriver::EnterKeypadCode - entered '%s'"), *Code);
	return true;
}
