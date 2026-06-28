// Fill out your copyright notice in the Description page of Project Settings.


#include "MovieBox.h"
#include "DiegeticTextComponent.h"
#include "FirstPersonCharacter.h"
#include "Inventory.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"

// Sets default values
AMovieBox::AMovieBox()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMovieBox::BeginPlay()
{
	Super::BeginPlay();

	// GetDefaultSubobjectByName doesn't reliably find Blueprint SCS-added components,
	// so iterate the actor's component list by name like we do for CantCarryWidget below.
	TArray<UWidgetComponent*> AllWidgets;
	GetComponents<UWidgetComponent>(AllWidgets);
	for (UWidgetComponent* Comp : AllWidgets)
	{
		if (Comp->GetFName() == TEXT("InteractionText"))
		{
			InteractionWidget = Comp;
			break;
		}
	}
	// Every real MovieBox carries an InteractionText widget. (BP_Spawner1, which used to
	// mis-inherit AMovieBox without one, is now a plain AActor.) A missing widget is a
	// setup error, not a state to silently recover from.
	checkf(InteractionWidget, TEXT("MovieBox %s is missing its InteractionText widget"), *GetName());

	EnvelopeMesh = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("Cube")));
	if (!EnvelopeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("Envelope Mesh component not found!"));
		return;
	}

	// Auto-load cover material based on actor name if not already set
	if (!CoverMaterial)
	{
		FString ActorName = GetName();
		// Strip the _N index suffix added by spawner (e.g., "12-MONKEYS_5" -> "12-MONKEYS")
		int32 LastUnderscore;
		if (ActorName.FindLastChar('_', LastUnderscore))
		{
			FString Suffix = ActorName.Mid(LastUnderscore + 1);
			if (Suffix.IsNumeric())
			{
				ActorName = ActorName.Left(LastUnderscore);
			}
		}

		FString MaterialPath = FString::Printf(TEXT("/Game/CreatedMaterials/VHSCoverMaterials/MI_VHSCover_%s"), *ActorName);
		CoverMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
	}

	if (CoverMaterial)
	{
		EnvelopeMesh->SetMaterial(0, CoverMaterial);
	}

	MyCharacter = Cast<AMyCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!MyCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("MyCharacter not found!"));
		return;
	}

	// Hide it initially
	InteractionWidget->SetVisibility(false);

	TArray<UTextRenderComponent*> AllTextRenders;
	GetComponents<UTextRenderComponent>(AllTextRenders);
	for (UTextRenderComponent* Comp : AllTextRenders)
	{
		if (Comp->GetFName() == TEXT("CantCarryText"))
		{
			CantCarryWidget = Comp;
			break;
		}
	}
	if (!CantCarryWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("CantCarryWidget component not found — add 'CantCarryText' widget component to BP_MovieBox"));
	}
	else
	{
		CantCarryWidget->SetVisibility(false);
	}

	// Put-back prompt is a "PutBackPromptText" UDiegeticTextComponent placed in
	// BP_MovieBox so its position is tunable in-editor. Fetch it by name, the
	// same way the other BP components above are wired up. (The component can't
	// be named "PutBackPrompt" — that collides with this class's member var.)
	TArray<UDiegeticTextComponent*> AllDiegeticText;
	GetComponents<UDiegeticTextComponent>(AllDiegeticText);
	for (UDiegeticTextComponent* Comp : AllDiegeticText)
	{
		if (Comp->GetFName() == TEXT("PutBackPromptText"))
		{
			PutBackPrompt = Comp;
			break;
		}
	}
	if (!PutBackPrompt)
	{
		UE_LOG(LogTemp, Warning, TEXT("MovieBox %s: PutBackPromptText component not found — add a 'PutBackPromptText' DiegeticText component to BP_MovieBox"), *GetName());
	}
	else
	{
		PutBackPrompt->SetText(FText::GetEmpty());
		PutBackPrompt->SetVisibility(false);
	}
}

// Called every frame
void AMovieBox::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Re-render the prompt if the player switched devices mid-inspection.
	// Only while it's actually showing — once it's been seen, it stays hidden.
	if (InspectedActor && PutBackPrompt && PutBackPrompt->IsVisible() && MyCharacter)
	{
		const AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(MyCharacter);
		if (FPChar && FPChar->IsUsingGamepad() != bPromptBuiltForGamepad)
		{
			bPromptBuiltForGamepad = FPChar->IsUsingGamepad();
			PutBackPrompt->SetText(FText::FromString(BuildPutBackPromptText()));
		}
	}
}

bool AMovieBox::CanInteract()
{
	return MyCharacter != nullptr;
}

void AMovieBox::Interact_Implementation()
{
	if (!InteractionWidget || !EnvelopeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieBox %s: Interact called with missing components — BeginPlay failed to initialize"), *GetName());
		return;
	}
	if (!MyCharacter)
	{
		return;
	}

	// Get the player's controller
	PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController)
		return;

	APawn* PlayerPawn = PlayerController->GetPawn();
	if (!PlayerPawn)
		return;

	// Get the camera component (assuming it's a first-person character with a camera)
	FVector CameraLocation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	// Store the actor's original transform before moving it
	OriginalActorTransform = GetActorTransform();

	// Offset distance in front of the camera
	FVector NewLocation = CameraLocation + (CameraRotation.Vector() * InspectionDistance);

	// Calculate rotation so the actor's X-axis (forward vector) faces the camera
	FRotator NewRotation = (CameraLocation - NewLocation).Rotation();

	// Set the actor's new position and rotation
	SetActorLocation(NewLocation);
	SetActorRotation(NewRotation);
	SetActorHiddenInGame(false);

	// Store reference to inspected actor (this MovieBox)
	InspectedActor = this;

	// Freeze player camera and movement; also ensures the PC InputComponent exists.
	MyCharacter->BeginInteractionHold(/*bFreezeLook*/ true);

	// Bind rotation input
	PlayerController->InputComponent->BindAxis("Turn Right / Left Mouse", this, &AMovieBox::RotateInspectedActor);
	PlayerController->InputComponent->BindAxis("Turn Right / Left Gamepad", this, &AMovieBox::RotateInspectedActor);

	// Defer the collect binding by one tick so the in-flight E IE_Pressed event
	// that opened inspection doesn't immediately trigger collect.
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (PlayerController && PlayerController->InputComponent)
			{
				PlayerController->InputComponent->BindAction(
					"Collect Inspected Movie", IE_Pressed,
					this, &AMovieBox::CollectInspectedMovie);
			}
		}));
	PlayerController->InputComponent->BindAction("Exit Interaction", IE_Pressed, this, &AMovieBox::StopInspection);

	// Show the collect prompt for the entire inspection if collection is allowed.
	// bExemptFromMovieLimit (e.g. BP_BlankVHS) bypasses both the cap and the lock.
	const bool bCanCollect = MyCharacter
		&& (bExemptFromMovieLimit
			|| (MyCharacter->GetInventoryComponent()->GetItemCount() < 3
				&& !MyCharacter->IsMovieCollectionLocked()));
	InteractionWidget->SetVisibility(bCanCollect);

	// The put-back prompt is a one-time control hint: show it only until the
	// player's first completed movie interaction (collecting or putting back).
	if (PutBackPrompt && MyCharacter && !MyCharacter->HasInteractedWithMovie())
	{
		const AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(MyCharacter);
		bPromptBuiltForGamepad = FPChar && FPChar->IsUsingGamepad();
		PutBackPrompt->SetText(FText::FromString(BuildPutBackPromptText()));
		PutBackPrompt->SetVisibility(true);
	}
}

FString AMovieBox::BuildPutBackPromptText() const
{
	if (!PlayerController || !PlayerController->PlayerInput)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieBox %s: no PlayerInput to read the put-back binding from"), *GetName());
		return FString();
	}

	auto ShortName = [](const FKey& Key) -> FString
	{
		if (Key == EKeys::Gamepad_FaceButton_Bottom) return FString(TEXT("A"));
		if (Key == EKeys::Gamepad_FaceButton_Right)  return FString(TEXT("B"));
		if (Key == EKeys::Gamepad_FaceButton_Left)   return FString(TEXT("X"));
		if (Key == EKeys::Gamepad_FaceButton_Top)    return FString(TEXT("Y"));
		return Key.GetDisplayName(false).ToString();
	};

	FString Keyboard, Gamepad;
	for (const FInputActionKeyMapping& Mapping : PlayerController->PlayerInput->GetKeysForAction(FName("Exit Interaction")))
	{
		if (Mapping.Key.IsGamepadKey())
		{
			if (Gamepad.IsEmpty()) Gamepad = ShortName(Mapping.Key);
		}
		else if (Keyboard.IsEmpty())
		{
			Keyboard = Mapping.Key.GetDisplayName(false).ToString();
		}
	}

	// Show only the binding for the device the player is actually using.
	const AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(MyCharacter);
	const bool bGamepad = FPChar && FPChar->IsUsingGamepad();
	const FString& Key = bGamepad ? Gamepad : Keyboard;
	if (Key.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("MovieBox %s: 'Exit Interaction' has no %s mapping"),
			*GetName(), bGamepad ? TEXT("gamepad") : TEXT("keyboard"));
		return FString();
	}
	return FString::Printf(TEXT("[%s]  put back"), *Key);
}

void AMovieBox::CollectInspectedMovie()
{
	if (!InteractionWidget || !EnvelopeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieBox %s: CollectInspectedMovie called with missing components"), *GetName());
		return;
	}
	if (DidCollectMovie) return;

	if (!bExemptFromMovieLimit)
	{
		if (MyCharacter && MyCharacter->IsMovieCollectionLocked())
		{
			if (CantCarryWidget)
			{
				CantCarryWidget->SetVisibility(true);
				GetWorldTimerManager().SetTimer(CantCarryTimerHandle,
					FTimerDelegate::CreateWeakLambda(this, [this]()
					{
						if (CantCarryWidget) CantCarryWidget->SetVisibility(false);
					}), 2.0f, false);
			}
			return;
		}

		if (MyCharacter && MyCharacter->GetInventoryComponent()->GetItemCount() >= 3)
		{
			if (CantCarryWidget)
			{
				CantCarryWidget->SetVisibility(true);
				GetWorldTimerManager().SetTimer(CantCarryTimerHandle,
					FTimerDelegate::CreateWeakLambda(this, [this]()
					{
						if (CantCarryWidget) CantCarryWidget->SetVisibility(false);
					}), 2.0f, false);
			}
			return;
		}
	}

	// SetVisibility(false, true) propagates to children so subclass-added child meshes
	// (e.g. BP_BlankVHS's "Tape") hide along with the parent Cube.
	EnvelopeMesh->SetVisibility(false, true);
	// The invisible box must not keep blocking interact traces — otherwise a
	// player aiming at the empty slot re-inspects an empty envelope.
	SetActorEnableCollision(false);
	InteractionWidget->SetVisibility(false);
	DidCollectMovie = true;

	// Get cover name from actor name (strip suffix)
	FString CoverName = InspectedActor ? InspectedActor->GetName() : GetName();
	int32 LastUnderscore;
	if (CoverName.FindLastChar('_', LastUnderscore))
	{
		const FString Suffix = CoverName.Mid(LastUnderscore + 1);
		if (Suffix.IsNumeric())
		{
			CoverName = CoverName.Left(LastUnderscore);
		}
	}

	// Add item to inventory with visual data captured from the envelope mesh.
	// World scale (not relative) so meshes whose size comes from ancestor
	// scaling are captured at the size the player actually saw.
	const FName InventoryID = !ItemIDOverride.IsNone() ? ItemIDOverride : FName(*CoverName);
	FInventoryItemData ItemData = UInventoryComponent::CreateItemDataFromMeshComponent(InventoryID, EnvelopeMesh);
	ItemData.Scale = EnvelopeMesh->GetComponentScale();
	if (UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent())
	{
		Inventory->AddItemWithData(ItemData);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MovieBox %s: player has no InventoryComponent"), *GetName());
	}

	// Defer StopInspection by one tick. 5.7 fires legacy "Collect Inspected Movie"
	// (IE_Pressed, this binding) before Enhanced Input IA_Interact (Triggered) on
	// the same E press; restoring CanInteract synchronously lets the subsequent
	// IA_Interact path raycast the shelf and re-open inspection.
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			StopInspection();
		}));
}

void AMovieBox::RotateInspectedActor(float AxisValue)
{
	if (!InspectedActor)
		return;

	FVector LocalUpVector = InspectedActor->GetActorUpVector();
	FQuat DeltaRotation = FQuat(LocalUpVector, FMath::DegreesToRadians(-AxisValue * 2.0f));
	InspectedActor->AddActorWorldRotation(DeltaRotation);
}


void AMovieBox::StopInspection()
{
	if (!InspectedActor)
		return;

	PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController)
		return;

	// Restore the object's original position and rotation
	InspectedActor->SetActorTransform(OriginalActorTransform);

	// Unbind input actions. Remove only the axes registered in Interact_Implementation
	// so other systems' axis bindings on the PC stay intact.
	PlayerController->InputComponent->AxisBindings.RemoveAll([](const FInputAxisBinding& Binding)
	{
		return Binding.AxisName == TEXT("Turn Right / Left Mouse")
			|| Binding.AxisName == TEXT("Turn Right / Left Gamepad");
	});
	PlayerController->InputComponent->RemoveActionBinding("Exit Interaction", IE_Pressed);
	PlayerController->InputComponent->RemoveActionBinding("Collect Inspected Movie", IE_Pressed);

	// Hide the collect and put-back prompts. The put-back prompt also clears
	// its text and returns to the pivot so it stops contributing stale
	// world-position bounds to the actor's bounding box.
	if (InteractionWidget) InteractionWidget->SetVisibility(false);
	if (PutBackPrompt)
	{
		PutBackPrompt->SetVisibility(false);
		PutBackPrompt->SetText(FText::GetEmpty());
	}

	// Clear inspected actor reference
	InspectedActor = nullptr;

	GetWorldTimerManager().ClearTimer(CantCarryTimerHandle);
	if (CantCarryWidget) CantCarryWidget->SetVisibility(false);

	MyCharacter->EndInteractionHold(/*bUnfreezeLook*/ true);

	// Both exits (collect and put-back) funnel through here, so this marks the
	// player's first completed movie interaction — the put-back hint stops
	// showing on every future inspection.
	MyCharacter->MarkMovieInteraction();
}

