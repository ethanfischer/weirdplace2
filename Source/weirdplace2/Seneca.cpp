#include "Seneca.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "BPFL_Utilities.h"
#include "Door.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ChildActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DlgSystem/DlgContext.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "UI_Dialogue.h"
#include "FirstPersonCharacter.h"

ASeneca::ASeneca()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// Components are created in Blueprint to preserve MetaHuman setup
}

void ASeneca::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("Seneca::BeginPlay - Participant name: '%s'"), *GetParticipantName_Implementation().ToString());

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

	if (TriggerSphere)
	{
		TriggerSphere->SetSphereRadius(DialogueTriggerRadius);
		TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ASeneca::OnSphereBeginOverlap);
		TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &ASeneca::OnSphereEndOverlap);
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
	LoadDialogueFile(ESenecaState::ReadyToGiveKey, ReadyToGiveKeyDialoguePath);
	LoadDialogueFile(ESenecaState::GaveKey, GaveKeyDialoguePath);
	LoadDialogueFile(ESenecaState::Smoking, SmokingDialoguePath);
	LoadDialogueFile(ESenecaState::AtEmployeeBathroom, EmployeeBathroomDialoguePath);
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
		for (const FString& Line : Lines)
		{
			if (!Line.IsEmpty())
			{
				TextLines.Add(FText::FromString(Line));
			}
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
	if (CurrentState != ESenecaState::WaitingForMovies)
	{
		return;
	}

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter))
	{
		if (UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent())
		{
			if (Inventory->GetItemCount() >= RequiredMovieCount)
			{
				CurrentState = ESenecaState::ReadyToGiveKey;
				UE_LOG(LogTemp, Log, TEXT("Seneca - State: WaitingForMovies -> ReadyToGiveKey (player has %d items)"), Inventory->GetItemCount());
			}
		}
	}
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
		break;
	}

	case ESenecaState::ReadyToGiveKey:
	{
		APlayerController* PC2 = GetWorld()->GetFirstPlayerController();
		if (PC2)
		{
			if (AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(PC2->GetPawn()))
			{
				FPChar->OnDialogueLineShown.RemoveDynamic(this, &ASeneca::OnKeyDialogueLineShown);
			}
		}
		GiveKey();
		CurrentState = ESenecaState::GaveKey;
		UE_LOG(LogTemp, Log, TEXT("Seneca - State: ReadyToGiveKey -> GaveKey"));
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
	SetActorHiddenInGame(true);
	if (TriggerSphere)
	{
		TriggerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
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
			TArray<FText> ReminderLines = { FText::FromString(TEXT("You need to rent 3 movies.")) };
			FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), ReminderLines, this);
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

void ASeneca::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (BodyMesh)
	{
		UBPFL_Utilities::SetShouldLookAtPlayer(true, OtherActor, BodyMesh);
	}

	AFirstPersonCharacter* FPCharacter = Cast<AFirstPersonCharacter>(OtherActor);
	if (!FPCharacter)
	{
		return;
	}

	if (CurrentState == ESenecaState::WaitingForMovies)
	{
		if (!bIntroDialoguePlayed)
			StartWaitingForMoviesDialogue(FPCharacter);
		return;
	}

	if (CurrentState == ESenecaState::ReadyToGiveKey)
	{
		StartReadyToGiveKeyDialogue(FPCharacter);
		return;
	}

	const TArray<FText>* Lines = GetDialogueLinesForCurrentState();
	if (Lines && Lines->Num() > 0)
	{
		FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), *Lines, this);
	}
}

void ASeneca::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (BodyMesh)
	{
		UBPFL_Utilities::SetShouldLookAtPlayer(false, OtherActor, BodyMesh);
	}
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

	// Waiting to appear at smoking position — teleport when player isn't looking at that spot
	if (bWaitingToAppear && SmokingPositionTarget)
	{
		if (!IsPlayerLookingAt(SmokingPositionTarget->GetActorLocation()))
		{
			MoveToTarget(SmokingPositionTarget);
			SetActorHiddenInGame(false);
			if (TriggerSphere)
			{
				TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			}
			bWaitingToAppear = false;
			bIsSmoking = true;
			if (CigaretteComp) CigaretteComp->SetVisibility(true, true);
			SetActorTickEnabled(false);
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
		SetActorTickEnabled(false);

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
	if (BodyMesh)
	{
		SenecaCenter = BodyMesh->Bounds.Origin;
	}
	else
	{
		SenecaCenter = GetActorLocation() + FVector(0, 0, 90.f);
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
	FPChar->StartSimpleDialogueMultiSpeaker(MultiLines, this);
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
		FPChar->bBlockNextMultiSpeakerAdvance = true;
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
			FPC->SetActivityState(EPlayerActivityState::InMultiSpeakerDialogue);
			FPC->AdvanceMultiSpeakerDialogue();
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
	FPChar->StartSimpleDialogueMultiSpeaker(MultiLines, this);
}

void ASeneca::OnKeyDialogueLineShown(int32 LineIndex)
{
	if (LineIndex != KeyBeatLineIndex)
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
		UE_LOG(LogTemp, Log, TEXT("Seneca::OnKeyDialogueLineShown - First broadcast (LineIndex=%d), arming beat"), LineIndex);
		bKeyBeatArmed = true;
		FPChar->bBlockNextMultiSpeakerAdvance = true;
		return;
	}

	// Second broadcast: show the key, switch to WaitingForItemInteractionInDialogue so the interaction system handles dismissal
	UE_LOG(LogTemp, Log, TEXT("Seneca::OnKeyDialogueLineShown - Second broadcast, binding lambda. KeyActor=%s"),
		KeyActor ? *KeyActor->GetName() : TEXT("NULL"));
	bKeyBeatArmed = false;

	if (!KeyActor)
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::OnKeyDialogueLineShown - KeyActor is not assigned"));
		return;
	}

	KeyActor->MeshComponent->SetVisibility(true, true);
	KeyActor->MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	FPChar->SetActivityState(EPlayerActivityState::WaitingForItemInteractionInDialogue);

	TWeakObjectPtr<APropActor> WeakProp(KeyActor);
	TWeakObjectPtr<AFirstPersonCharacter> WeakFPChar(FPChar);

	KeyActor->OnInteracted.AddLambda([WeakProp, WeakFPChar]()
	{
		UE_LOG(LogTemp, Log, TEXT("Seneca - Key OnInteracted lambda fired"));
		if (APropActor* P = WeakProp.Get())
		{
			P->MeshComponent->SetVisibility(false, true);
			P->MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P->OnInteracted.Clear();
		}
		if (AFirstPersonCharacter* FPC = WeakFPChar.Get())
		{
			FPC->SetActivityState(EPlayerActivityState::InMultiSpeakerDialogue);
			FPC->AdvanceMultiSpeakerDialogue();
		}
	});
}

// --- IDlgDialogueParticipant (minimal stubs - logic lives in C++ state machine) ---

FName ASeneca::GetParticipantName_Implementation() const
{
	return FName("Seneca");
}

FText ASeneca::GetParticipantDisplayName_Implementation(FName ActiveSpeaker) const
{
	return FText::FromString("Seneca");
}

ETextGender ASeneca::GetParticipantGender_Implementation() const
{
	return ETextGender::Masculine;
}

UTexture2D* ASeneca::GetParticipantIcon_Implementation(FName ActiveSpeaker, FName ActiveSpeakerState) const
{
	return nullptr;
}

bool ASeneca::CheckCondition_Implementation(const UDlgContext* Context, FName ConditionName) const
{
	return false;
}

float ASeneca::GetFloatValue_Implementation(FName ValueName) const
{
	return 0.0f;
}

int32 ASeneca::GetIntValue_Implementation(FName ValueName) const
{
	return 0;
}

bool ASeneca::GetBoolValue_Implementation(FName ValueName) const
{
	return false;
}

FName ASeneca::GetNameValue_Implementation(FName ValueName) const
{
	return NAME_None;
}

bool ASeneca::OnDialogueEvent_Implementation(UDlgContext* Context, FName EventName)
{
	return false;
}

bool ASeneca::ModifyFloatValue_Implementation(FName ValueName, bool bDelta, float Value)
{
	return false;
}

bool ASeneca::ModifyIntValue_Implementation(FName ValueName, bool bDelta, int32 Value)
{
	return false;
}

bool ASeneca::ModifyBoolValue_Implementation(FName ValueName, bool bNewValue)
{
	return false;
}

bool ASeneca::ModifyNameValue_Implementation(FName ValueName, FName NameValue)
{
	return false;
}
