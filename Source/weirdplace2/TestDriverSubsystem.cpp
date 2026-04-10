#include "TestDriverSubsystem.h"
#include "FirstPersonCharacter.h"
#include "Interactable.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "MovieBox.h"
#include "PropActor.h"
#include "Rick.h"
#include "Seneca.h"
#include "TestWaypoint.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

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

	const FVector Target = Waypoint->GetActorLocation();
	const FRotator TargetRot = Waypoint->GetActorRotation();

	Player->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);

	// Set the controller's rotation so the camera follows; the first-person
	// camera uses bUsePawnControlRotation, so the controller drives view angle.
	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		PC->SetControlRotation(TargetRot);
	}

	UE_LOG(LogTemp, Log, TEXT("TestDriver::TeleportPlayerToWaypoint - '%s' at %s"), *WaypointTag.ToString(), *Target.ToString());
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

	// Aim at the target's center height from the camera. This mirrors how
	// the interaction trace runs (from camera forward at camera height).
	const FVector CamLoc = Camera->GetComponentLocation();
	const FVector TargetLoc = Target->GetActorLocation();
	const FVector Dir = (TargetLoc - CamLoc).GetSafeNormal();
	const FRotator NewRot = Dir.Rotation();

	PC->SetControlRotation(NewRot);
	UE_LOG(LogTemp, Log, TEXT("TestDriver::LookAt - %s"), *Target->GetName());
	return true;
}

void UTestDriverSubsystem::PressInteract()
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::PressInteract - no player"));
		return;
	}

	// Replicate HandleInteractTriggered's branching. That method is private,
	// so we can't call it directly — but every branch here resolves to a
	// public gameplay API.
	const EPlayerActivityState State = Player->GetActivityState();
	if (State == EPlayerActivityState::InSimpleDialogue)
	{
		Player->AdvanceSimpleDialogue();
		return;
	}
	if (State == EPlayerActivityState::InMultiSpeakerDialogue)
	{
		Player->AdvanceMultiSpeakerDialogue();
		return;
	}
	if (State == EPlayerActivityState::InDlgDialogue)
	{
		Player->SelectDialogueOption(0);
		return;
	}

	if (!Player->GetCanInteract())
	{
		UE_LOG(LogTemp, Warning, TEXT("TestDriver::PressInteract - player cannot interact (state=%d)"), (int32)State);
		return;
	}

	AActor* HitActor = nullptr;
	bool bDidHitInteractable = false;
	Player->RaycastInteractableCheck(HitActor, bDidHitInteractable);

	if (!bDidHitInteractable || !HitActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("TestDriver::PressInteract - no interactable hit"));
		return;
	}

	if (HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		IInteractable::Execute_Interact(HitActor);
		UE_LOG(LogTemp, Log, TEXT("TestDriver::PressInteract - fired Interact on %s"), *HitActor->GetName());
	}
}

bool UTestDriverSubsystem::InteractWith(AActor* Actor)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InteractWith - null actor"));
		return false;
	}
	if (!Actor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InteractWith - actor %s does not implement IInteractable"), *Actor->GetName());
		return false;
	}
	IInteractable::Execute_Interact(Actor);
	UE_LOG(LogTemp, Log, TEXT("TestDriver::InteractWith - fired Interact on %s"), *Actor->GetName());
	return true;
}

bool UTestDriverSubsystem::InteractWithSeneca()
{
	ASeneca* Seneca = FindSeneca();
	if (!Seneca)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InteractWithSeneca - no ASeneca in level"));
		return false;
	}
	return InteractWith(Seneca);
}

bool UTestDriverSubsystem::InteractWithRick()
{
	ARick* Rick = FindRick();
	if (!Rick)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InteractWithRick - no ARick in level"));
		return false;
	}
	return InteractWith(Rick);
}

bool UTestDriverSubsystem::InteractWithKeyActor()
{
	ASeneca* Seneca = FindSeneca();
	if (!Seneca)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InteractWithKeyActor - no ASeneca in level"));
		return false;
	}
	APropActor* Key = Seneca->GetKeyActor();
	if (!Key)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InteractWithKeyActor - KeyActor not assigned on Seneca"));
		return false;
	}
	return InteractWith(Key);
}

bool UTestDriverSubsystem::InteractWithActorByLabel(const FString& Label)
{
	AActor* Found = FindActorByLabel(Label);
	if (!Found)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::InteractWithActorByLabel - no actor '%s'"), *Label);
		return false;
	}
	return InteractWith(Found);
}

AActor* UTestDriverSubsystem::GetFocusedInteractable() const
{
	AFirstPersonCharacter* Player = GetPlayer();
	if (!Player)
	{
		return nullptr;
	}

	AActor* HitActor = nullptr;
	bool bDidHitInteractable = false;
	Player->RaycastInteractableCheck(HitActor, bDidHitInteractable);
	return bDidHitInteractable ? HitActor : nullptr;
}

void UTestDriverSubsystem::OpenInventory()
{
	UInventoryUIComponent* UI = GetInventoryUIComponent();
	if (!UI)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::OpenInventory - no InventoryUIComponent"));
		return;
	}
	UI->OpenInventoryUI();
}

void UTestDriverSubsystem::CloseInventory()
{
	UInventoryUIComponent* UI = GetInventoryUIComponent();
	if (!UI)
	{
		return;
	}
	UI->CloseInventoryUI();
}

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

bool UTestDriverSubsystem::SelectInventoryItemByIndex(int32 Index)
{
	UInventoryUIComponent* UI = GetInventoryUIComponent();
	if (!UI)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::SelectInventoryItemByIndex - no UI component"));
		return false;
	}
	if (!UI->IsInventoryFullyOpen())
	{
		UE_LOG(LogTemp, Warning, TEXT("TestDriver::SelectInventoryItemByIndex - inventory not fully open"));
		return false;
	}

	UI->SetSelectedIndexForTest(Index);
	UI->ConfirmSelection();
	return true;
}

AMovieBox* UTestDriverSubsystem::CollectNextMovie()
{
	// Drop any stale weak refs so the set doesn't grow unbounded.
	for (auto It = CollectedMovies.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// Pick any uncollected movie in the level. Order is iterator order, which
	// is deterministic per run. Distance doesn't matter because we call Interact
	// directly, bypassing the interaction trace.
	//
	// Skip actors without an InteractionText widget — BP_Spawner1 is incorrectly
	// parented to BP_MovieBox, so TActorIterator<AMovieBox> picks it up even
	// though it's a spawner, not a usable movie.
	AMovieBox* Best = nullptr;
	for (TActorIterator<AMovieBox> It(GetWorld()); It; ++It)
	{
		if (CollectedMovies.Contains(*It))
		{
			continue;
		}
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
		Best = *It;
		break;
	}

	if (!Best)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::CollectNextMovie - no uncollected MovieBox in level"));
		return nullptr;
	}

	// Interact enters inspection mode (sets activity state + moves box in front
	// of camera). CollectInspectedSubitem adds the item to inventory and exits
	// inspection. We call both directly to bypass the reticle/rotation gate
	// that the real input binding uses.
	const int32 CountBefore = GetInventoryCount();
	IInteractable::Execute_Interact(Best);
	Best->CollectInspectedSubitem();
	const int32 CountAfter = GetInventoryCount();

	if (CountAfter != CountBefore + 1)
	{
		UE_LOG(LogTemp, Error, TEXT("TestDriver::CollectNextMovie - inventory did not grow (%d -> %d)"), CountBefore, CountAfter);
		return nullptr;
	}

	CollectedMovies.Add(Best);
	UE_LOG(LogTemp, Log, TEXT("TestDriver::CollectNextMovie - collected %s"), *Best->GetName());
	return Best;
}

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
