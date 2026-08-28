#include "Seneca.h"
#include "PropActor.h"
#include "StorySubsystem.h"
#include "FirstPersonCharacter.h"
#include "InventoryUIComponent.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "Door.h"
#include "GazeUtils.h"
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
				if (DialogueWidgetComponent)
				{
					// A semi-transparent dialogue backing plate (UUI_Dialogue) needs
					// alpha blending. Force Transparent blend so BackingOpacity is a
					// true gradient -- Masked blend clips it binary at the 0.333 mask
					// threshold, which reads as a hard step near ~0.35.
					DialogueWidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);
				}
			}
			break;
		}
	}

	// The dark plate behind the dialogue text now lives inside the shared
	// UUI_Dialogue widget (a UBorder that auto-hugs the text), so it covers
	// Seneca, Rick and Hudson at once — no per-actor world-space backing here.

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
		UE_LOG(LogTemp, Warning, TEXT("Seneca::BeginPlay - No SkeletalMeshComponent found"));
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

	// Load + parse the sectioned dialogue file
	DialogueScript.Load(DialogueFilePath);
	LoadDialogueSection(ESenecaState::WaitingForMovies, WaitingForMoviesSection);
	LoadDialogueSection(ESenecaState::WaitingForMoviePurchase, WaitingForMoviePurchaseSection);
	LoadDialogueSection(ESenecaState::WaitingForMoney, WaitingForMoneySection);
	LoadDialogueSection(ESenecaState::WaitingForBlankTape, WaitingForBlankTapeSection);
	LoadDialogueSection(ESenecaState::AwaitingTapeBurn, AwaitingTapeBurnSection);
	LoadDialogueSection(ESenecaState::ReadyToGiveCombinedTape, ReadyToGiveCombinedTapeSection);
	LoadDialogueSection(ESenecaState::ReadyToGiveKey, ReadyToGiveKeySection);
	LoadDialogueSection(ESenecaState::GaveKey, GaveKeySection);
	LoadDialogueSection(ESenecaState::Smoking, SmokingSection);

	// Reminder lines (not keyed by state)
	LoadReminderSection(WaitingForMoviesReminderSection, WaitingForMoviesReminderLines);
	LoadReminderSection(WaitingForMoviePurchaseReminderSection, WaitingForMoviePurchaseReminderLines);
	LoadReminderSection(WaitingForBlankTapeReminderSection, WaitingForBlankTapeReminderLines);

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

void ASeneca::LoadDialogueSection(ESenecaState State, const FString& SectionName)
{
	const TArray<FDialogueLine>* Section = DialogueScript.FindSection(SectionName);
	if (!Section)
	{
		return;
	}

	TArray<FText>& TextLines = DialogueLines.Add(State);
	TMap<int32, FString>& Actions = LineActions.Add(State);
	for (const FDialogueLine& Line : *Section)
	{
		TextLines.Add(FText::FromString(Line.Text));
		if (!Line.Tag.IsEmpty())
		{
			Actions.Add(TextLines.Num() - 1, Line.Tag);
		}
	}
}

void ASeneca::LoadReminderSection(const FString& SectionName, TArray<FText>& OutLines)
{
	const TArray<FDialogueLine>* Section = DialogueScript.FindSection(SectionName);
	if (!Section)
	{
		return;
	}
	for (const FDialogueLine& Line : *Section)
	{
		OutLines.Add(FText::FromString(Line.Text));
	}
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
			if (FPChar3)
			{
				FPChar3->LockMovieCollection();
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
		// Smoking dialogue is now purely cosmetic — Seneca stays at the smoking spot
		// and unlocks nothing. The employee bathroom is opened via the phone-code
		// keypad on the door, not by Seneca.
		CurrentState = ESenecaState::Done;
		UE_LOG(LogTemp, Log, TEXT("Seneca - Smoking dialogue ended (cosmetic), State: Smoking -> Done"));
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
		UInventoryComponent* Inventory = FPCharacter->GetInventoryComponent();
		if (!Inventory)
		{
			UE_LOG(LogTemp, Error, TEXT("Seneca::Interact - Could not get inventory for WaitingForMoviePurchase"));
			return;
		}

		// Have a movie? Pop the inventory so the player picks which one to hand over
		// (one per interact). Otherwise remind them. A "movie" is any non-tool item.
		if (FindFirstMovie(Inventory).IsNone())
		{
			FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Seneca")), WaitingForMoviePurchaseReminderLines, this);
		}
		else
		{
			OpenGiveForOffer(FPCharacter);
		}
		return;
	}

	if (CurrentState == ESenecaState::WaitingForMoney)
	{
		UInventoryComponent* Inventory = FPCharacter->GetInventoryComponent();
		if (Inventory && Inventory->HasItem(FName("Money")))
		{
			OpenGiveForOffer(FPCharacter);
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
		UInventoryComponent* Inventory = FPCharacter->GetInventoryComponent();
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

		const FName ChosenID = CachedMovieSpawner->GetChosenItemID();
		if (!ChosenID.IsNone() && Inventory->HasItem(ChosenID))
		{
			OpenGiveForOffer(FPCharacter);
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

	// Waiting to appear at smoking position — teleport when player isn't looking at
	// that spot, but only once the player has used the payphone at least once.
	// (Key drop and payphone use can happen in either order; Tick keeps polling
	// until both conditions hold.)
	if (bWaitingToAppear && SmokingPositionTarget)
	{
		const UWorld* World = GetWorld();
		const UStorySubsystem* Story = World ? World->GetSubsystem<UStorySubsystem>() : nullptr;
		if (!Story || !Story->IsFlagSet(EStoryFlag::UsedPayPhone))
		{
			return;
		}
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
}

bool ASeneca::IsPlayerLookingAt(const FVector& Position) const
{
	// Shared cone test (~60 degree half-angle). Seneca deliberately uses the
	// point/cone variant, not the ray-box IsActorInPlayerGaze: the smoking marker
	// is a near-zero-bounds actor, so a box test would read as "looking" almost
	// never and change the look-away teleport feel.
	return UGazeUtils::IsPointInPlayerView(Position, GetWorld());
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
	TArray<FString> Lines;
	if (!FDialogueScript::LoadRawLines(MovieCommentsPath, Lines))
	{
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
	const TArray<FDialogueLine>* Section = DialogueScript.FindSection(MoviePurchaseSection);
	if (!Section)
	{
		return;
	}

	TArray<FSimpleDialogueLine> MultiLines;
	for (const FDialogueLine& Parsed : *Section)
	{
		FSimpleDialogueLine Line;
		Line.Speaker = FText::FromString(Parsed.Speaker.IsEmpty() ? TEXT("Seneca") : *Parsed.Speaker);
		Line.Text = FText::FromString(Parsed.Text);
		Line.PauseAfter = Parsed.PauseAfter;
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

void ASeneca::HandleBlankTapeGive(AFirstPersonCharacter* FPChar, UInventoryComponent* Inventory, FName BlankTapeID)
{
	if (!Inventory->RemoveItem(BlankTapeID))
	{
		UE_LOG(LogTemp, Error, TEXT("Seneca::HandleBlankTapeGive - Failed to remove '%s' from inventory"), *BlankTapeID.ToString());
		return;
	}

	CurrentState = ESenecaState::ReadyToGiveKey;
	UE_LOG(LogTemp, Log, TEXT("Seneca - State: WaitingForBlankTape -> ReadyToGiveKey (blank tape '%s' received; burn off-screen)"), *BlankTapeID.ToString());

	StartReadyToGiveKeyDialogue(FPChar);
}

bool ASeneca::IsMovieItem(FName ItemID)
{
	// A "movie" is any inventory item that isn't one of the fixed tool/quest items.
	static const TSet<FName> ToolItems = {
		FName("Key"), FName("BrokenKey"), FName("Money"), FName("BlankVHS"), FName("CombinedTape")
	};
	return !ItemID.IsNone() && !ToolItems.Contains(ItemID);
}

FName ASeneca::FindFirstMovie(UInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return NAME_None;
	}
	for (const FName& ItemID : Inventory->GetItems())
	{
		if (IsMovieItem(ItemID))
		{
			return ItemID;
		}
	}
	return NAME_None;
}

void ASeneca::OpenGiveForOffer(AFirstPersonCharacter* MyCharacter)
{
	if (!MyCharacter)
	{
		return;
	}
	if (UInventoryUIComponent* InvUI = MyCharacter->GetInventoryUIComponent())
	{
		InvUI->OpenForGive(FInventoryGiveDelegate::CreateUObject(this, &ASeneca::OnInventoryItemOffered));
	}
}

bool ASeneca::OnInventoryItemOffered(FName ItemID)
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(PlayerCharacter);
	UInventoryComponent* Inventory = FPChar ? FPChar->GetInventoryComponent() : nullptr;
	if (!FPChar || !Inventory)
	{
		return false;
	}
	// Validate the offered item for the current state; on accept, consume + advance
	// and return true (ConfirmGiveSelection closes the UI). Wrong item -> return
	// false (stay open).
	switch (CurrentState)
	{
	case ESenecaState::WaitingForMoviePurchase:
		if (IsMovieItem(ItemID) && Inventory->HasItem(ItemID))
		{
			HandleMovieGive(FPChar, Inventory, ItemID);
			return true;
		}
		return false;

	case ESenecaState::WaitingForMoney:
		if (ItemID == FName("Money"))
		{
			Inventory->RemoveItem(FName("Money"));
			CurrentState = ESenecaState::WaitingForBlankTape;
			UE_LOG(LogTemp, Log, TEXT("Seneca - State: WaitingForMoney -> WaitingForBlankTape (money received)"));
			if (CachedMovieSpawner)
			{
				CachedMovieSpawner->ActivateChosenTape();
			}
			StartWaitingForBlankTapeDialogue(FPChar);
			return true;
		}
		return false;

	case ESenecaState::WaitingForBlankTape:
		if (CachedMovieSpawner && ItemID == CachedMovieSpawner->GetChosenItemID())
		{
			HandleBlankTapeGive(FPChar, Inventory, ItemID);
			return true;
		}
		return false;

	default:
		return false;
	}
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
	MoveToTarget(SmokingPositionTarget);
	SetActorEnableCollision(true);
	bIsSmoking = true;
	if (CigaretteComp) CigaretteComp->SetVisibility(true, true);
	StartSmokingAnim();
	UE_LOG(LogTemp, Log, TEXT("Seneca::ForceSmokingAppearance - teleported + smoking"));
}

