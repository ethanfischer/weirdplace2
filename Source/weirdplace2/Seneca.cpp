#include "Seneca.h"
#include "PropActor.h"
#include "StorySubsystem.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "Door.h"
#include "SpawnerActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ChildActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "UI_Dialogue.h"
#include "FirstPersonCharacter.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Components/PointLightComponent.h"

ASeneca::ASeneca()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// Components are created in Blueprint to preserve MetaHuman setup
}

UUI_Dialogue* ASeneca::GetDialogueWidget() const
{
	if (!DialogueWidgetComponent)
	{
		return nullptr;
	}
	return Cast<UUI_Dialogue>(DialogueWidgetComponent->GetUserWidgetObject());
}

void ASeneca::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("Seneca::BeginPlay"));

	// Find the dialogue widget component - it's inside a Child Actor Component
	TArray<UChildActorComponent*> ChildActorComponents;
	GetComponents<UChildActorComponent>(ChildActorComponents);
	for (UChildActorComponent* ChildActorComp : ChildActorComponents)
	{
		if (ChildActorComp->GetName().Contains(TEXT("WorldSpace_UI_Dialogue")))
		{
			if (AActor* ChildActor = ChildActorComp->GetChildActor())
			{
				DialogueWidgetComponent = ChildActor->FindComponentByClass<UWidgetComponent>();
			}
			break;
		}
	}

	// Listen for inventory changes to auto-advance WaitingForMovies → ReadyToGiveKey
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter))
	{
		if (UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent())
		{
			Inventory->OnInventoryChanged.AddDynamic(this, &ASeneca::OnInventoryChanged);
		}
	}

	// Find the Cigarette ChildActorComponent by name
	TArray<UChildActorComponent*> ChildActorComps;
	GetComponents<UChildActorComponent>(ChildActorComps);
	for (UChildActorComponent* ChildActorComp : ChildActorComps)
	{
		if (ChildActorComp->GetName().Contains(TEXT("Cigarette")))
		{
			CigaretteComp = ChildActorComp;
			break;
		}
	}
	if (!CigaretteComp)
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::BeginPlay - Could not find Cigarette ChildActorComponent"));
	}

	// Cache the skeletal mesh for bounds-based look-at targeting
	CachedSkeletalMesh = FindComponentByClass<USkeletalMeshComponent>();
	if (!CachedSkeletalMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::BeginPlay - No SkeletalMeshComponent found, IsPlayerLookingAtMe will use hardcoded offset"));
	}

	// Smoking-spot point light starts off; toggled by Start/StopSmokingAnim.
	if (UPointLightComponent* Light = FindComponentByClass<UPointLightComponent>())
	{
		Light->SetVisibility(false);
	}

	if (KeyActor)
	{
		FTimerHandle HideKeyHandle;
		GetWorldTimerManager().SetTimer(HideKeyHandle, [this]()
		{
			if (KeyActor)
			{
				KeyActor->MeshComponent->SetVisibility(false, true);
				KeyActor->MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}, 0.1f, false);
	}

	if (ShoppingBasketActor)
	{
		FTimerHandle HideBasketHandle;
		GetWorldTimerManager().SetTimer(HideBasketHandle, [this]()
		{
			if (ShoppingBasketActor)
			{
				UE_LOG(LogTemp, Log, TEXT("Seneca - Hiding ShoppingBasketActor: %s"), *ShoppingBasketActor->GetName());
				ShoppingBasketActor->MeshComponent->SetVisibility(false, true);
				ShoppingBasketActor->MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}, 0.1f, false);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::BeginPlay - ShoppingBasketActor is not assigned"));
	}

	// Load dialogue text files
	LoadDialogueFile(ESenecaState::WaitingForMovies, WaitingForMoviesDialoguePath);
	LoadDialogueFile(ESenecaState::WaitingForMoviePurchase, WaitingForMoviePurchaseDialoguePath);
	LoadDialogueFile(ESenecaState::WaitingForMoney, WaitingForMoneyDialoguePath);
	LoadDialogueFile(ESenecaState::WaitingForBlankTape, WaitingForBlankTapeDialoguePath);
	LoadDialogueFile(ESenecaState::AwaitingTapeBurn, AwaitingTapeBurnDialoguePath);
	LoadDialogueFile(ESenecaState::ReadyToGiveCombinedTape, ReadyToGiveCombinedTapeDialoguePath);
	LoadDialogueFile(ESenecaState::ReadyToGiveKey, ReadyToGiveKeyDialoguePath);
	LoadDialogueFile(ESenecaState::GaveKey, GaveKeyDialoguePath);
	LoadDialogueFile(ESenecaState::Smoking, SmokingDialoguePath);
	LoadDialogueFile(ESenecaState::AtEmployeeBathroom, EmployeeBathroomDialoguePath);

	// Load reminder lines (not keyed by state)
	{
		auto LoadReminderFile = [](const FString& RelativePath, TArray<FText>& OutLines)
		{
			FString FullPath = FPaths::ProjectContentDir() / RelativePath;
			TArray<FString> Raw;
			if (FFileHelper::LoadFileToStringArray(Raw, *FullPath))
			{
				for (const FString& Line : Raw)
				{
					if (!Line.IsEmpty())
					{
						OutLines.Add(FText::FromString(Line));
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Seneca - Failed to load reminder file: %s"), *FullPath);
			}
		};
		LoadReminderFile(WaitingForMoviesReminderPath, WaitingForMoviesReminderLines);
		LoadReminderFile(WaitingForMoviePurchaseReminderPath, WaitingForMoviePurchaseReminderLines);
		LoadReminderFile(WaitingForBlankTapeReminderPath, WaitingForBlankTapeReminderLines);
	}

	LoadMovieComments();

	if (MovieSpawnerActor)
	{
		CachedMovieSpawner = MovieSpawnerActor->FindComponentByClass<USpawnerActorComponent>();
		if (!CachedMovieSpawner)
		{
			UE_LOG(LogTemp, Error, TEXT("Seneca::BeginPlay - MovieSpawnerActor %s has no USpawnerActorComponent"), *MovieSpawnerActor->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::BeginPlay - MovieSpawnerActor not assigned on level instance"));
	}
}

// --- State Machine ---

const TArray<FText>* ASeneca::GetDialogueLinesForCurrentState() const
{
	return DialogueLines.Find(CurrentState);
}

void ASeneca::BuildEffectiveDialogueLines(ESenecaState State, TArray<FText>& Out) const
{
	Out.Reset();
	if (const TArray<FText>* Lines = DialogueLines.Find(State))
	{
		Out = *Lines;
	}

	// Smoking gains a tornado-shelter tip once the player has actually seen the
	// warning on the store TVs. Gated on SeenTornadoWarning so it never leaks
	// into a playthrough where the TVs were never watched.
	if (State == ESenecaState::Smoking)
	{
		const UWorld* World = GetWorld();
		const UStorySubsystem* Story = World ? World->GetSubsystem<UStorySubsystem>() : nullptr;
		if (Story && Story->IsFlagSet(EStoryFlag::SeenTornadoWarning))
		{
			Out.Add(FText::FromString(TEXT("And listen -- that twister's no joke. There's a shelter under the far stall if it comes to that.")));
		}
	}
}

void ASeneca::LoadDialogueFile(ESenecaState State, const FString& RelativePath)
{
	FString FullPath = FPaths::ProjectContentDir() / RelativePath;
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToStringArray(Lines, *FullPath))
	{
		TArray<FText>& TextLines = DialogueLines.Add(State);
		TMap<int32, FString>& Actions = LineActions.Add(State);
		for (const FString& Raw : Lines)
		{
			FString Line = Raw;
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty())
			{
				continue;
			}

			// `[Action Name]` lines mark a cue tied to the immediately preceding display line.
			if (Line.StartsWith(TEXT("[")) && Line.EndsWith(TEXT("]")))
			{
				if (TextLines.Num() == 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("Seneca - Action cue '%s' in %s has no preceding dialogue line; ignoring"), *Line, *FullPath);
					continue;
				}
				FString ActionName = Line.Mid(1, Line.Len() - 2).TrimStartAndEnd();
				Actions.Add(TextLines.Num() - 1, ActionName);
				continue;
			}

			TextLines.Add(FText::FromString(Line));
		}

		UE_LOG(LogTemp, Log, TEXT("Seneca - Loaded %d lines from %s"), TextLines.Num(), *FullPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca - Failed to load dialogue file: %s"), *FullPath);
	}
}

void ASeneca::CheckMovieCount()
{
	// State transition now happens in OnDialogueEnded (WaitingForMovies case)
}

void ASeneca::OnInventoryChanged(const TArray<FName>& CurrentItems)
{
	CheckMovieCount();
}

void ASeneca::OnDialogueEnded()
{
	UE_LOG(LogTemp, Log, TEXT("Seneca::OnDialogueEnded - CurrentState: %d"), static_cast<int32>(CurrentState));

	switch (CurrentState)
	{
	case ESenecaState::WaitingForMovies:
	{
		bIntroDialoguePlayed = true;
		CurrentState = ESenecaState::WaitingForMoviePurchase;
		UE_LOG(LogTemp, Log, TEXT("Seneca - State: WaitingForMovies -> WaitingForMoviePurchase (intro dialogue done)"));
		break;
	}

	case ESenecaState::WaitingForMoviePurchase:
	{
		APlayerController* PC3 = GetWorld()->GetFirstPlayerController();
		AFirstPersonCharacter* FPChar3 = PC3 ? Cast<AFirstPersonCharacter>(PC3->GetPawn()) : nullptr;
		if (MoviesGivenCount >= RequiredMovieCount && FPChar3)
		{
			CurrentState = ESenecaState::WaitingForMoney;
			UE_LOG(LogTemp, Log, TEXT("Seneca - State: WaitingForMoviePurchase -> WaitingForMoney"));
			ACharacter* PC2 = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
			if (AMyCharacter* MC = Cast<AMyCharacter>(PC2))
			{
				MC->LockMovieCollection();
			}
			StartMoviePurchaseDialogue(FPChar3);
		}
		break;
	}

	case ESenecaState::ReadyToGiveCombinedTape:
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (AFirstPersonCharacter* FPChar = PC ? Cast<AFirstPersonCharacter>(PC->GetPawn()) : nullptr)
		{
			FPChar->OnDialogueLineShown.RemoveDynamic(this, &ASeneca::OnCombinedTapeDialogueLineShown);
		}
		CurrentState = ESenecaState::ReadyToGiveKey;
		UE_LOG(LogTemp, Log, TEXT("Seneca - State: ReadyToGiveCombinedTape -> ReadyToGiveKey"));
		break;
	}

	case ESenecaState::ReadyToGiveKey:
	{
		APlayerController* PC2 = GetWorld()->GetFirstPlayerController();
		AFirstPersonCharacter* FPChar = PC2 ? Cast<AFirstPersonCharacter>(PC2->GetPawn()) : nullptr;
		if (FPChar)
		{
			FPChar->OnDialogueLineShown.RemoveDynamic(this, &ASeneca::OnKeyDialogueLineShown);
		}
		CurrentState = ESenecaState::GaveKey;
		UE_LOG(LogTemp, Log, TEXT("Seneca - State: ReadyToGiveKey -> GaveKey"));
		if (FPChar)
		{
			const TArray<FText>* GaveKeyLines = DialogueLines.Find(ESenecaState::GaveKey);
			if (GaveKeyLines && GaveKeyLines->Num() > 0)
			{
				FPChar->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), *GaveKeyLines, this);
			}
		}
		break;
	}

	case ESenecaState::Smoking:
		// Defer move until player looks away
		PendingMoveTarget = EmployeeBathroomPositionTarget;
		bWasLookingAtMe = false;
		SetActorTickEnabled(true);
		UE_LOG(LogTemp, Log, TEXT("Seneca - Smoking dialogue ended, waiting for player to look away"));
		break;

	case ESenecaState::AtEmployeeBathroom:
		if (EmployeeBathroomDoor)
		{
			EmployeeBathroomDoor->SetLocked(false);
			UE_LOG(LogTemp, Log, TEXT("Seneca - Unlocked employee bathroom door"));
		}
		CurrentState = ESenecaState::Done;
		UE_LOG(LogTemp, Log, TEXT("Seneca - State: AtEmployeeBathroom -> Done"));
		break;

	default:
		break;
	}
}

void ASeneca::OnKeyDropped()
{
	UE_LOG(LogTemp, Log, TEXT("Seneca::OnKeyDropped - Hiding, will appear at smoking position in %.0f seconds"), SmokingAppearDelay);
	// Teleport far below the world instead of toggling visibility — visibility
	// cycling on a MetaHuman puts the groom hair into a bad state on re-show.
	SetActorLocation(FVector(0.0, 0.0, -100000.0));
	SetActorEnableCollision(false);
	CurrentState = ESenecaState::Smoking;

	// The broken key now enters the inventory via a collectable AInspectablePickup
	// that carries BrokenKeyDef's own thumbnail, so there's nothing to override
	// here. (This used to run while the door auto-added the broken key; under the
	// pickup flow it ran before the key existed and logged a spurious error.)

	GetWorldTimerManager().SetTimer(SmokingAppearTimerHandle, this, &ASeneca::OnSmokingDelayComplete, SmokingAppearDelay, false);
}

void ASeneca::OnSmokingDelayComplete()
{
	UE_LOG(LogTemp, Log, TEXT("Seneca - Smoking delay complete, waiting for player to look away from smoking spot"));
	bWaitingToAppear = true;
	SetActorTickEnabled(true);
}

void ASeneca::FastForwardSmokingAppear()
{
	UE_LOG(LogTemp, Log, TEXT("Seneca::FastForwardSmokingAppear - skipping SmokingAppearDelay"));
	if (GetWorldTimerManager().IsTimerActive(SmokingAppearTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(SmokingAppearTimerHandle);
	}
	OnSmokingDelayComplete();
}

// --- Interaction ---

void ASeneca::Interact_Implementation()
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AFirstPersonCharacter* FPCharacter = Cast<AFirstPersonCharacter>(PlayerCharacter);
	if (!FPCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::Interact - Player is not AFirstPersonCharacter"));
		return;
	}

	if (CurrentState == ESenecaState::WaitingForMovies)
	{
		if (!bIntroDialoguePlayed)
		{
			StartWaitingForMoviesDialogue(FPCharacter);
		}
		else
		{
			FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), WaitingForMoviesReminderLines, this);
		}
		return;
	}

	if (CurrentState == ESenecaState::WaitingForMoviePurchase)
	{
		AMyCharacter* MyCharacter = Cast<AMyCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
		UInventoryComponent* Inventory = MyCharacter ? MyCharacter->GetInventoryComponent() : nullptr;
		if (!Inventory)
		{
			UE_LOG(LogTemp, Error, TEXT("Seneca::Interact - Could not get inventory for WaitingForMoviePurchase"));
			return;
		}

		FName ActiveItem = Inventory->GetActiveItem();
		if (ActiveItem.IsNone() || ActiveItem == FName("Key") || ActiveItem == FName("BrokenKey"))
		{
			FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), WaitingForMoviePurchaseReminderLines, this);
		}
		else
		{
			HandleMovieGive(FPCharacter, Inventory, ActiveItem);
		}
		return;
	}

	if (CurrentState == ESenecaState::WaitingForMoney)
	{
		AMyCharacter* MyCharacter = Cast<AMyCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
		UInventoryComponent* Inventory = MyCharacter ? MyCharacter->GetInventoryComponent() : nullptr;
		if (Inventory && Inventory->GetActiveItem() == FName("Money"))
		{
			if (!Inventory->RemoveItem(FName("Money")))
			{
				UE_LOG(LogTemp, Error, TEXT("Seneca - Failed to remove Money from inventory"));
				return;
			}
			Inventory->ClearActiveItem();
			CurrentState = ESenecaState::WaitingForBlankTape;
			UE_LOG(LogTemp, Log, TEXT("Seneca - State: WaitingForMoney -> WaitingForBlankTape (money received)"));
			if (CachedMovieSpawner)
			{
				UE_LOG(LogTemp, Log, TEXT("Seneca - Activating chord-spawner chosen tape"));
				CachedMovieSpawner->ActivateChosenTape();
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Seneca - CachedMovieSpawner not set; cannot activate chosen tape"));
			}
			StartWaitingForBlankTapeDialogue(FPCharacter);
		}
		else
		{
			const TArray<FText>* Lines = DialogueLines.Find(ESenecaState::WaitingForMoney);
			if (Lines && Lines->Num() > 0)
			{
				FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), *Lines, this);
			}
		}
		return;
	}

	if (CurrentState == ESenecaState::WaitingForBlankTape)
	{
		AMyCharacter* MyCharacter = Cast<AMyCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
		UInventoryComponent* Inventory = MyCharacter ? MyCharacter->GetInventoryComponent() : nullptr;
		if (!Inventory)
		{
			UE_LOG(LogTemp, Error, TEXT("Seneca::Interact - Could not get inventory for WaitingForBlankTape"));
			return;
		}

		if (!CachedMovieSpawner)
		{
			UE_LOG(LogTemp, Error, TEXT("Seneca::Interact - CachedMovieSpawner not set; cannot identify blank tape"));
			return;
		}

		const FName ActiveItem = Inventory->GetActiveItem();
		const FName ChosenID = CachedMovieSpawner->GetChosenItemID();
		if (!ActiveItem.IsNone() && !ChosenID.IsNone() && ActiveItem == ChosenID)
		{
			HandleBlankTapeGive(FPCharacter, Inventory);
		}
		else
		{
			FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), WaitingForBlankTapeReminderLines, this);
		}
		return;
	}

	if (CurrentState == ESenecaState::AwaitingTapeBurn)
	{
		const TArray<FText>* Lines = DialogueLines.Find(ESenecaState::AwaitingTapeBurn);
		if (Lines && Lines->Num() > 0)
		{
			FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), *Lines, this);
		}
		return;
	}

	if (CurrentState == ESenecaState::ReadyToGiveCombinedTape)
	{
		StartReadyToGiveCombinedTapeDialogue(FPCharacter);
		return;
	}

	if (CurrentState == ESenecaState::ReadyToGiveKey)
	{
		StartReadyToGiveKeyDialogue(FPCharacter);
		return;
	}

	TArray<FText> EffectiveLines;
	BuildEffectiveDialogueLines(CurrentState, EffectiveLines);
	if (EffectiveLines.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Seneca::Interact - No dialogue for state %d"), static_cast<int32>(CurrentState));
		return;
	}

	FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), EffectiveLines, this);
}

// --- Key ---

void ASeneca::GiveKey(AFirstPersonCharacter* FPChar)
{
	UE_LOG(LogTemp, Log, TEXT("Seneca::GiveKey called"));

	if (!FPChar)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveKey - FPChar is null"));
		return;
	}

	UInventoryComponent* Inventory = FPChar->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveKey - Player has no InventoryComponent"));
		return;
	}

	if (!KeyDef || KeyDef->ItemID.IsNone() || !KeyDef->Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::GiveKey - KeyDef missing/incomplete"));
		return;
	}

	FInventoryItemData ItemData = KeyDef->ToInventoryItemData();
	Inventory->AddItemWithData(ItemData);
	FPChar->ShowItemNotification(ItemData, KeyDef->NotificationRotation);
	UE_LOG(LogTemp, Log, TEXT("Seneca::GiveKey - Gave key '%s' to player"), *KeyDef->ItemID.ToString());
}

// --- Helpers ---

void ASeneca::MoveToTarget(AActor* Target)
{
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::MoveToTarget - Target is null"));
		return;
	}

	SetActorLocation(Target->GetActorLocation());
	SetActorRotation(Target->GetActorRotation());
	UE_LOG(LogTemp, Log, TEXT("Seneca::MoveToTarget - Moved to %s"), *Target->GetName());
}

void ASeneca::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Billboard dialogue widget toward player camera
	if (DialogueWidgetComponent)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			FVector CamLocation;
			FRotator CamRotation;
			PC->GetPlayerViewPoint(CamLocation, CamRotation);
			FVector WidgetLocation = DialogueWidgetComponent->GetComponentLocation();
			FRotator LookAtRot = (CamLocation - WidgetLocation).Rotation();
			DialogueWidgetComponent->SetWorldRotation(LookAtRot);
		}
	}

	// Waiting to appear at smoking position — teleport when player isn't looking at that spot
	if (bWaitingToAppear && SmokingPositionTarget)
	{
		if (!IsPlayerLookingAt(SmokingPositionTarget->GetActorLocation()))
		{
			MoveToTarget(SmokingPositionTarget);
			SetActorEnableCollision(true);
			bWaitingToAppear = false;
			bIsSmoking = true;
			if (CigaretteComp) CigaretteComp->SetVisibility(true, true);
			StartSmokingAnim();
			UE_LOG(LogTemp, Log, TEXT("Seneca - Appeared at smoking position"));
		}
		return;
	}

	// Waiting for player to look away so Seneca can teleport to employee bathroom
	if (!PendingMoveTarget)
	{
		return;
	}

	bool bLooking = IsPlayerLookingAtMe();

	if (bLooking)
	{
		bWasLookingAtMe = true;
	}
	else if (bWasLookingAtMe)
	{
		UE_LOG(LogTemp, Log, TEXT("Seneca - Player looked away, moving to employee bathroom"));
		MoveToTarget(PendingMoveTarget);
		bIsSmoking = false;
		StopSmokingAnim();
		if (CigaretteComp) CigaretteComp->SetVisibility(false, true);
		PendingMoveTarget = nullptr;
		bWasLookingAtMe = false;

		CurrentState = ESenecaState::AtEmployeeBathroom;
		UE_LOG(LogTemp, Log, TEXT("Seneca - State: Smoking -> AtEmployeeBathroom"));
	}
}

bool ASeneca::IsPlayerLookingAt(const FVector& Position) const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	FVector CameraLoc;
	FRotator CameraRot;
	PC->GetPlayerViewPoint(CameraLoc, CameraRot);

	FVector ToTarget = (Position - CameraLoc).GetSafeNormal();
	FVector CameraForward = CameraRot.Vector();

	float Dot = FVector::DotProduct(CameraForward, ToTarget);
	// ~60 degree half-angle cone
	return Dot > 0.5f;
}

bool ASeneca::IsPlayerLookingAtMe() const
{
	FVector SenecaCenter;
	if (CachedSkeletalMesh)
	{
		FBoxSphereBounds LocalBounds = CachedSkeletalMesh->GetLocalBounds();
		// Upper-center of mesh bounds in world space
		const FVector LocalUpperCenter = LocalBounds.Origin + FVector(0.f, 0.f, LocalBounds.BoxExtent.Z);
		SenecaCenter = CachedSkeletalMesh->GetComponentTransform().TransformPosition(LocalUpperCenter);
	}
	else
	{
		SenecaCenter = GetActorLocation() + FVector(0.f, 0.f, 90.f);
	}
	return IsPlayerLookingAt(SenecaCenter);
}

// --- Basket Beat ---

void ASeneca::StartWaitingForMoviesDialogue(AFirstPersonCharacter* FPChar)
{
	const TArray<FText>* Lines = DialogueLines.Find(ESenecaState::WaitingForMovies);
	if (!Lines || Lines->Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::StartWaitingForMoviesDialogue - No lines found"));
		return;
	}

	TArray<FSimpleDialogueLine> MultiLines;
	for (const FText& LineText : *Lines)
	{
		FSimpleDialogueLine Line;
		Line.Speaker = FText::FromString(TEXT("Seneca"));
		Line.Text = LineText;
		MultiLines.Add(Line);
	}

	FPChar->StartDialogue(MultiLines, this);
}

// --- Key Beat ---

void ASeneca::StartReadyToGiveKeyDialogue(AFirstPersonCharacter* FPChar)
{
	const TArray<FText>* Lines = DialogueLines.Find(ESenecaState::ReadyToGiveKey);
	if (!Lines || Lines->Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::StartReadyToGiveKeyDialogue - No lines found"));
		return;
	}

	TArray<FSimpleDialogueLine> MultiLines;
	for (const FText& LineText : *Lines)
	{
		FSimpleDialogueLine Line;
		Line.Speaker = FText::FromString(TEXT("Seneca"));
		Line.Text = LineText;
		MultiLines.Add(Line);
	}

	FPChar->OnDialogueLineShown.RemoveDynamic(this, &ASeneca::OnKeyDialogueLineShown);
	FPChar->OnDialogueLineShown.AddDynamic(this, &ASeneca::OnKeyDialogueLineShown);
	FPChar->StartDialogue(MultiLines, this);
}

void ASeneca::OnKeyDialogueLineShown(int32 LineIndex)
{
	const FString Action = GetActionForLine(ESenecaState::ReadyToGiveKey, LineIndex);
	if (Action.IsEmpty())
	{
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(PC->GetPawn());
	if (!FPChar)
	{
		return;
	}

	if (!bKeyBeatArmed)
	{
		// First broadcast: arm the block so the next E press triggers the beat
		UE_LOG(LogTemp, Log, TEXT("Seneca::OnKeyDialogueLineShown - Arming '%s' beat at LineIndex=%d"), *Action, LineIndex);
		bKeyBeatArmed = true;
		FPChar->bBlockNextDialogueAdvance = true;
		return;
	}

	// Second broadcast: execute the action.
	UE_LOG(LogTemp, Log, TEXT("Seneca::OnKeyDialogueLineShown - Executing '%s'"), *Action);
	bKeyBeatArmed = false;

	if (Action == TEXT("Give key"))
	{
		GiveKey(FPChar);
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("Seneca::OnKeyDialogueLineShown - Unknown action '%s'"), *Action);
}

// --- Movie Purchase Beat ---

void ASeneca::LoadMovieComments()
{
	FString FullPath = FPaths::ProjectContentDir() / MovieCommentsPath;
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::LoadMovieComments - Failed to load: %s"), *FullPath);
		return;
	}

	for (const FString& Line : Lines)
	{
		if (Line.IsEmpty()) continue;

		FString Key, Comment;
		if (!Line.Split(TEXT(": "), &Key, &Comment))
		{
			UE_LOG(LogTemp, Warning, TEXT("Seneca::LoadMovieComments - Skipping malformed line: %s"), *Line);
			continue;
		}

		Key.TrimStartAndEndInline();
		Comment.TrimStartAndEndInline();

		if (Key == TEXT("FALLBACK"))
		{
			FallbackMovieComment = FText::FromString(Comment);
		}
		else
		{
			MovieComments.Add(FName(*Key), FText::FromString(Comment));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Seneca::LoadMovieComments - Loaded %d comments (+fallback)"), MovieComments.Num());
}

FString ASeneca::GetActionForLine(ESenecaState State, int32 LineIndex) const
{
	const TMap<int32, FString>* Actions = LineActions.Find(State);
	if (!Actions)
	{
		return FString();
	}
	const FString* Found = Actions->Find(LineIndex);
	return Found ? *Found : FString();
}

// --- Counter Stack ---

void ASeneca::PlaceMovieOnCounter(const FInventoryItemData& MovieData)
{
	if (!CounterStackPosition)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::PlaceMovieOnCounter - CounterStackPosition not assigned"));
		return;
	}
	if (!MovieData.Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::PlaceMovieOnCounter - MovieData has no mesh"));
		return;
	}

	const FVector Location = CounterStackPosition->GetActorLocation()
		+ CounterStackPosition->GetActorUpVector() * MovieStackHeight * CounterMovieActors.Num();
	const FRotator Rotation = CounterStackPosition->GetActorRotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APropActor* Prop = GetWorld()->SpawnActor<APropActor>(APropActor::StaticClass(), Location, Rotation, Params);
	if (!Prop)
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::PlaceMovieOnCounter - Failed to spawn counter prop"));
		return;
	}

	Prop->MeshComponent->SetStaticMesh(MovieData.Mesh);
	for (int32 i = 0; i < MovieData.Materials.Num(); i++)
	{
		Prop->MeshComponent->SetMaterial(i, MovieData.Materials[i]);
	}
	Prop->MeshComponent->SetRelativeScale3D(MovieData.Scale);
	Prop->MeshComponent->SetRelativeRotation(MovieRelativeRotation);
	Prop->MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CounterMovieActors.Add(Prop);
	UE_LOG(LogTemp, Log, TEXT("Seneca::PlaceMovieOnCounter - Placed movie %d at %s scale=%s mesh=%s"),
		CounterMovieActors.Num(), *Location.ToString(), *MovieData.Scale.ToString(),
		MovieData.Mesh ? *MovieData.Mesh->GetName() : TEXT("null"));
}

void ASeneca::ClearCounterMovies()
{
	for (AActor* Prop : CounterMovieActors)
	{
		if (Prop)
		{
			Prop->Destroy();
		}
	}
	CounterMovieActors.Empty();
	UE_LOG(LogTemp, Log, TEXT("Seneca::ClearCounterMovies - Cleared counter"));
}

void ASeneca::HandleMovieGive(AFirstPersonCharacter* FPChar, UInventoryComponent* Inventory, FName MovieID)
{
	const FText* Found = MovieComments.Find(MovieID);
	FText Comment = Found ? *Found : FallbackMovieComment;

	// Capture full item data so we can return the rented movies to the player later.
	TakenMovies.Add(Inventory->GetItemData(MovieID));
	PlaceMovieOnCounter(TakenMovies.Last());

	Inventory->RemoveItem(MovieID);
	Inventory->ClearActiveItem();
	MoviesGivenCount++;

	UE_LOG(LogTemp, Log, TEXT("Seneca::HandleMovieGive - Received '%s', MoviesGivenCount=%d"), *MovieID.ToString(), MoviesGivenCount);

	TArray<FText> CommentLines;
	FString Line1, Line2;
	if (Comment.ToString().Split(TEXT("|"), &Line1, &Line2))
	{
		CommentLines.Add(FText::FromString(Line1.TrimStartAndEnd()));
		CommentLines.Add(FText::FromString(Line2.TrimStartAndEnd()));
	}
	else
	{
		CommentLines.Add(Comment);
	}
	FPChar->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), CommentLines, this);
}

void ASeneca::StartMoviePurchaseDialogue(AFirstPersonCharacter* FPChar)
{
	FString FullPath = FPaths::ProjectContentDir() / MoviePurchaseDialoguePath;
	TArray<FString> RawLines;
	if (!FFileHelper::LoadFileToStringArray(RawLines, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::StartMoviePurchaseDialogue - Failed to load: %s"), *FullPath);
		return;
	}

	TArray<FSimpleDialogueLine> MultiLines;
	for (const FString& Raw : RawLines)
	{
		if (Raw.IsEmpty()) continue;

		FString Speaker, Text;
		if (!Raw.Split(TEXT(": "), &Speaker, &Text))
		{
			UE_LOG(LogTemp, Warning, TEXT("Seneca::StartMoviePurchaseDialogue - Skipping malformed line: %s"), *Raw);
			continue;
		}

		FSimpleDialogueLine Line;
		Line.Speaker = FText::FromString(Speaker.TrimStartAndEnd());
		Line.Text = FText::FromString(Text.TrimStartAndEnd());
		MultiLines.Add(Line);
	}

	FPChar->StartDialogue(MultiLines, this);
}

// --- Blank Tape Beat ---

void ASeneca::StartWaitingForBlankTapeDialogue(AFirstPersonCharacter* FPChar)
{
	const TArray<FText>* Lines = DialogueLines.Find(ESenecaState::WaitingForBlankTape);
	if (!Lines || Lines->Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::StartWaitingForBlankTapeDialogue - No lines found"));
		return;
	}
	FPChar->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), *Lines, this);
}

void ASeneca::HandleBlankTapeGive(AFirstPersonCharacter* FPChar, UInventoryComponent* Inventory)
{
	const FName ActiveItem = Inventory->GetActiveItem();
	if (!Inventory->RemoveItem(ActiveItem))
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::HandleBlankTapeGive - Failed to remove '%s' from inventory"), *ActiveItem.ToString());
		return;
	}
	Inventory->ClearActiveItem();

	CurrentState = ESenecaState::ReadyToGiveKey;
	UE_LOG(LogTemp, Log, TEXT("Seneca - State: WaitingForBlankTape -> ReadyToGiveKey (blank tape '%s' received; burn off-screen)"), *ActiveItem.ToString());

	StartReadyToGiveKeyDialogue(FPChar);
}

void ASeneca::StartAwaitingTapeBurnDialogue(AFirstPersonCharacter* FPChar)
{
	const TArray<FText>* Lines = DialogueLines.Find(ESenecaState::AwaitingTapeBurn);
	if (!Lines || Lines->Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::StartAwaitingTapeBurnDialogue - No lines found"));
		return;
	}
	FPChar->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), *Lines, this);
}

void ASeneca::GiveCombinedTape()
{
	if (CurrentState != ESenecaState::AwaitingTapeBurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveCombinedTape called in state %d; ignoring"),
			static_cast<int32>(CurrentState));
		return;
	}
	CurrentState = ESenecaState::ReadyToGiveCombinedTape;
	UE_LOG(LogTemp, Log, TEXT("Seneca - State: AwaitingTapeBurn -> ReadyToGiveCombinedTape (combined tape ready)"));
}

// --- Combined Tape Beat ---

void ASeneca::StartReadyToGiveCombinedTapeDialogue(AFirstPersonCharacter* FPChar)
{
	const TArray<FText>* Lines = DialogueLines.Find(ESenecaState::ReadyToGiveCombinedTape);
	if (!Lines || Lines->Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::StartReadyToGiveCombinedTapeDialogue - No lines found"));
		return;
	}

	TArray<FSimpleDialogueLine> MultiLines;
	for (const FText& LineText : *Lines)
	{
		FSimpleDialogueLine Line;
		Line.Speaker = FText::FromString(TEXT("Seneca"));
		Line.Text = LineText;
		MultiLines.Add(Line);
	}

	FPChar->OnDialogueLineShown.RemoveDynamic(this, &ASeneca::OnCombinedTapeDialogueLineShown);
	FPChar->OnDialogueLineShown.AddDynamic(this, &ASeneca::OnCombinedTapeDialogueLineShown);
	FPChar->StartDialogue(MultiLines, this);
}

void ASeneca::OnCombinedTapeDialogueLineShown(int32 LineIndex)
{
	const FString Action = GetActionForLine(ESenecaState::ReadyToGiveCombinedTape, LineIndex);
	if (Action.IsEmpty())
	{
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(PC->GetPawn());
	if (!FPChar)
	{
		return;
	}

	if (!bCombinedTapeBeatArmed)
	{
		UE_LOG(LogTemp, Log, TEXT("Seneca::OnCombinedTapeDialogueLineShown - Arming '%s' beat at LineIndex=%d"), *Action, LineIndex);
		bCombinedTapeBeatArmed = true;
		FPChar->bBlockNextDialogueAdvance = true;
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Seneca::OnCombinedTapeDialogueLineShown - Executing '%s'"), *Action);
	bCombinedTapeBeatArmed = false;

	if (Action == TEXT("Give combined tape"))
	{
		if (!CombinedTapeDef)
		{
			UE_LOG(LogTemp, Error, TEXT("Seneca::OnCombinedTapeDialogueLineShown - CombinedTapeDef not assigned"));
			return;
		}

		UInventoryComponent* Inventory = FPChar->GetInventoryComponent();
		if (!Inventory)
		{
			UE_LOG(LogTemp, Error, TEXT("Seneca::OnCombinedTapeDialogueLineShown - No inventory; cannot give combined tape"));
			return;
		}

		FInventoryItemData ItemData = CombinedTapeDef->ToInventoryItemData();
		Inventory->AddItemWithData(ItemData);
		FPChar->ShowItemNotification(ItemData, CombinedTapeDef->NotificationRotation);
		TakenMovies.Reset();
		ClearCounterMovies();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("Seneca::OnCombinedTapeDialogueLineShown - Unknown action '%s'"), *Action);
}

USkeletalMeshComponent* ASeneca::FindBodyMesh() const
{
	if (!SmokingAnimation) return nullptr;
	USkeleton* WantSkeleton = SmokingAnimation->GetSkeleton();
	TArray<USkeletalMeshComponent*> Meshes;
	GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* M : Meshes)
	{
		if (!M || !M->GetSkeletalMeshAsset()) continue;
		if (M->GetSkeletalMeshAsset()->GetSkeleton() == WantSkeleton) return M;
	}
	return nullptr;
}

void ASeneca::StartSmokingAnim()
{
	if (UPointLightComponent* Light = FindComponentByClass<UPointLightComponent>())
	{
		Light->SetVisibility(true);
	}
	if (!SmokingAnimation)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::StartSmokingAnim - SmokingAnimation unset on BP_Seneca"));
		return;
	}
	USkeletalMeshComponent* Body = FindBodyMesh();
	if (!Body)
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::StartSmokingAnim - no body mesh matching SmokingAnimation's skeleton"));
		return;
	}
	Body->PlayAnimation(SmokingAnimation, true);
}

void ASeneca::StopSmokingAnim()
{
	if (UPointLightComponent* Light = FindComponentByClass<UPointLightComponent>())
	{
		Light->SetVisibility(false);
	}
	USkeletalMeshComponent* Body = FindBodyMesh();
	if (!Body) return;
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
}

void ASeneca::ForceSmokingAppearance()
{
	if (!SmokingPositionTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::ForceSmokingAppearance - SmokingPositionTarget unset on level instance"));
		return;
	}
	if (GetWorldTimerManager().IsTimerActive(SmokingAppearTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(SmokingAppearTimerHandle);
	}
	bWaitingToAppear = false;
	PendingMoveTarget = nullptr;
	MoveToTarget(SmokingPositionTarget);
	SetActorEnableCollision(true);
	bIsSmoking = true;
	if (CigaretteComp) CigaretteComp->SetVisibility(true, true);
	StartSmokingAnim();
	UE_LOG(LogTemp, Log, TEXT("Seneca::ForceSmokingAppearance - teleported + smoking"));
}

