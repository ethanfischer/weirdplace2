#include "Seneca.h"
#include "PropActor.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "Door.h"
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
	}

	LoadMovieComments();
}

// --- State Machine ---

const TArray<FText>* ASeneca::GetDialogueLinesForCurrentState() const
{
	return DialogueLines.Find(CurrentState);
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

		if (State == ESenecaState::WaitingForMovies)
		{
			for (FText& Line : TextLines)
			{
				FString Str = Line.ToString().Replace(TEXT("[[InventoryButton]]"), *InventoryButtonDisplayName);
				Line = FText::FromString(Str);
			}
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
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			if (AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(PC->GetPawn()))
			{
				FPChar->OnDialogueLineShown.RemoveDynamic(this, &ASeneca::OnBasketDialogueLineShown);
			}
		}

		ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
		if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter))
		{
			MyCharacter->UnlockInventory();
		}
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

	if (KeyBrokenThumbnail)
	{
		ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
		if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter))
		{
			if (UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent())
			{
				// OutsideBathroomDoor adds the broken key as "BrokenKey", not KeyToGive
				Inventory->UpdateItemThumbnail(FName("BrokenKey"), KeyBrokenThumbnail);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::OnKeyDropped - KeyBrokenThumbnail not assigned on level instance"));
	}

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

	if (FPCharacter->GetActivityState() == EPlayerActivityState::WaitingForItemInteractionInDialogue)
	{
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
			CurrentState = ESenecaState::ReadyToGiveKey;
			UE_LOG(LogTemp, Log, TEXT("Seneca - State: WaitingForMoney -> ReadyToGiveKey (money received)"));
			StartReadyToGiveKeyDialogue(FPCharacter);
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

	if (CurrentState == ESenecaState::ReadyToGiveKey)
	{
		StartReadyToGiveKeyDialogue(FPCharacter);
		return;
	}

	const TArray<FText>* Lines = GetDialogueLinesForCurrentState();
	if (!Lines || Lines->Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Seneca::Interact - No dialogue for state %d"), static_cast<int32>(CurrentState));
		return;
	}

	FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), *Lines, this);
}

// --- Key ---

void ASeneca::GiveKey()
{
	UE_LOG(LogTemp, Log, TEXT("Seneca::GiveKey called"));

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveKey - Failed to get player character"));
		return;
	}

	AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter);
	if (!MyCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveKey - Player is not AMyCharacter"));
		return;
	}

	UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveKey - Player has no InventoryComponent"));
		return;
	}

	if (KeyToGive == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Seneca::GiveKey - KeyToGive is NAME_None"));
		return;
	}

	if (!KeyMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::GiveKey - KeyMesh is not set"));
		return;
	}

	FInventoryItemData ItemData;
	ItemData.ItemID = KeyToGive;
	ItemData.Mesh = KeyMesh;
	ItemData.Scale = KeyScale;

	for (int32 i = 0; i < KeyMesh->GetStaticMaterials().Num(); i++)
	{
		ItemData.Materials.Add(KeyMesh->GetMaterial(i));
	}

	Inventory->AddItemWithData(ItemData);
	UE_LOG(LogTemp, Log, TEXT("Seneca::GiveKey - Gave key '%s' to player"), *KeyToGive.ToString());
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

	FPChar->OnDialogueLineShown.RemoveDynamic(this, &ASeneca::OnBasketDialogueLineShown);
	FPChar->OnDialogueLineShown.AddDynamic(this, &ASeneca::OnBasketDialogueLineShown);
	FPChar->StartDialogue(MultiLines, this);
}

void ASeneca::OnBasketDialogueLineShown(int32 LineIndex)
{
	if (LineIndex != BasketBeatLineIndex)
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

	if (!bBasketBeatArmed)
	{
		// First broadcast: arm the block so the next E press triggers the beat
		bBasketBeatArmed = true;
		FPChar->bBlockNextDialogueAdvance = true;
		return;
	}

	// Second broadcast: show the basket, switch to WaitingForItemInteractionInDialogue so the interaction system handles dismissal
	bBasketBeatArmed = false;

	if (!ShoppingBasketActor)
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::OnBasketDialogueLineShown - ShoppingBasketActor is not assigned"));
		return;
	}

	ShoppingBasketActor->MeshComponent->SetVisibility(true, true);
	ShoppingBasketActor->MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	FPChar->SetActivityState(EPlayerActivityState::WaitingForItemInteractionInDialogue);

	TWeakObjectPtr<APropActor> WeakProp(ShoppingBasketActor);
	TWeakObjectPtr<AFirstPersonCharacter> WeakFPChar(FPChar);

	ShoppingBasketActor->OnInteracted.AddLambda([WeakProp, WeakFPChar]()
	{
		if (APropActor* P = WeakProp.Get())
		{
			P->MeshComponent->SetVisibility(false, true);
			P->MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P->OnInteracted.Clear();
		}
		if (AFirstPersonCharacter* FPC = WeakFPChar.Get())
		{
			if (UInventoryComponent* Inv = FPC->GetInventoryComponent())
			{
				UGameplayStatics::PlaySound2D(FPC->GetWorld(), Inv->CollectSound);
			}
			FPC->SetActivityState(EPlayerActivityState::InDialogue);
			FPC->AdvanceDialogue();
		}
	});
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

	if (Action == TEXT("Give movies"))
	{
		UInventoryComponent* Inventory = FPChar->GetInventoryComponent();
		if (!Inventory)
		{
			UE_LOG(LogTemp, Error, TEXT("Seneca::OnKeyDialogueLineShown - No inventory; cannot return movies"));
			FPChar->AdvanceDialogue();
			return;
		}

		if (TakenMovies.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Seneca::OnKeyDialogueLineShown - 'Give movies' fired but TakenMovies is empty"));
		}

		for (const FInventoryItemData& MovieData : TakenMovies)
		{
			Inventory->AddItemWithData(MovieData);
		}
		TakenMovies.Reset();
		ClearCounterMovies();

		FPChar->AdvanceDialogue();
		return;
	}

	if (Action == TEXT("Give key"))
	{
		GiveKey();

		FInventoryItemData KeyData;
		KeyData.ItemID = KeyToGive;
		KeyData.Mesh = KeyMesh;
		KeyData.Scale = KeyScale;
		if (KeyMesh)
		{
			for (int32 i = 0; i < KeyMesh->GetStaticMaterials().Num(); i++)
			{
				KeyData.Materials.Add(KeyMesh->GetMaterial(i));
			}
		}
		FPChar->ShowItemNotification(KeyData);
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

