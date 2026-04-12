// Fill out your copyright notice in the Description page of Project Settings.


#include "MovieBox.h"
#include "Inventory.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"

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
	if (!InteractionWidget)
	{
		// BP_Spawner1 is parented to BP_MovieBox but doesn't have an InteractionText widget —
		// it shouldn't inherit from MovieBox at all, but until that's fixed in the editor we
		// bail out here so the spawner doesn't run the rest of MovieBox::BeginPlay.
		UE_LOG(LogTemp, Error, TEXT("MovieBox %s: InteractionText widget not found"), *GetName());
		return;
	}

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
}

// Called every frame
void AMovieBox::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool AMovieBox::CanInteract()
{
	return MyCharacter && MyCharacter->IsInventoryUnlocked();
}

void AMovieBox::Interact_Implementation()
{
	if (!InteractionWidget || !EnvelopeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieBox %s: Interact called with missing components — BeginPlay failed to initialize"), *GetName());
		return;
	}
	if (!MyCharacter || !MyCharacter->IsInventoryUnlocked())
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

	// Freeze player camera and movement
	PlayerController->SetIgnoreLookInput(true);
	PlayerController->SetIgnoreMoveInput(true);

	MyCharacter->SetCanInteract(false);
	MyCharacter->SetActivityState(EPlayerActivityState::Interacting);

	// Ensure input component exists
	if (!PlayerController->InputComponent)
	{
		PlayerController->InputComponent = NewObject<UInputComponent>(PlayerController);
		PlayerController->InputComponent->RegisterComponent();
	}

	// Bind rotation input
	PlayerController->InputComponent->BindAxis("Turn Right / Left Mouse", this, &AMovieBox::RotateInspectedActor);
	PlayerController->InputComponent->BindAxis("Turn Right / Left Gamepad", this, &AMovieBox::RotateInspectedActor);

	// Bind Q key to exit inspection
	PlayerController->InputComponent->BindAction("Exit Interaction", IE_Pressed, this, &AMovieBox::StopInspection);
	// Allow interact key (E) to also exit while inspecting
	PlayerController->InputComponent->BindAction(InteractActionName, IE_Pressed, this, &AMovieBox::StopInspection);
}

void AMovieBox::CollectInspectedSubitem()
{
	if (!InteractionWidget || !EnvelopeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieBox %s: CollectInspectedSubitem called with missing components"), *GetName());
		return;
	}
	if (DidCollectSubitem) return;

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

	EnvelopeMesh->SetHiddenInGame(true);
	InteractionWidget->SetVisibility(false);
	DidCollectSubitem = true;

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

	// Add item to inventory with visual data captured from the envelope mesh
	MyCharacter->AddItemToInventoryWithMesh(FName(*CoverName), EnvelopeMesh);

	// Close inspection after collecting
	StopInspection();
}

void AMovieBox::RotateInspectedActor(float AxisValue)
{
	if (!InspectedActor)
		return;
	double DotProduct = FVector::DotProduct(CameraRotation.Vector(), InspectedActor->GetActorForwardVector());
	//Use dot product to determine if movie box and player camera are facing the same world direction
	if (DotProduct > 0.9f)
	{
		if (!DidCollectSubitem)
		{
			bool bCanCollect = MyCharacter
			&& MyCharacter->GetInventoryComponent()->GetItemCount() < 3
			&& !MyCharacter->IsMovieCollectionLocked();
			InteractionWidget->SetVisibility(bCanCollect);
		}

		// Only bind if not already bound (prevent duplicate bindings)
		if (!bCollectSubitemBound)
		{
			PlayerController->InputComponent->BindAction("Collect Inspected Subitem", IE_Pressed, this, &AMovieBox::CollectInspectedSubitem);
			bCollectSubitemBound = true;
		}
	}
	else
	{
		InteractionWidget->SetVisibility(false);
		if (CantCarryWidget)
		{
			GetWorldTimerManager().ClearTimer(CantCarryTimerHandle);
			CantCarryWidget->SetVisibility(false);
		}

		// Only unbind if currently bound
		if (bCollectSubitemBound)
		{
			PlayerController->InputComponent->RemoveActionBinding("Collect Inspected Subitem", IE_Pressed);
			bCollectSubitemBound = false;
		}
	}

	// Get the local up vector of the actor
	FVector LocalUpVector = InspectedActor->GetActorUpVector();

	// Create a rotation around the local up axis
	FQuat DeltaRotation = FQuat(LocalUpVector, FMath::DegreesToRadians(-AxisValue * 2.0f)); // Adjust sensitivity

	// Apply the rotation in local space
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

	// Restore player movement and camera control
	PlayerController->SetIgnoreLookInput(false);
	PlayerController->SetIgnoreMoveInput(false);

	// Unbind input actions
	PlayerController->InputComponent->AxisBindings.Empty(); //TODO: really?
	RemoveInteractBinding();

	// Clear inspected actor reference
	InspectedActor = nullptr;

	// Reset binding flag
	if (bCollectSubitemBound)
	{
		PlayerController->InputComponent->RemoveActionBinding("Collect Inspected Subitem", IE_Pressed);
		bCollectSubitemBound = false;
	}

	GetWorldTimerManager().ClearTimer(CantCarryTimerHandle);
	if (CantCarryWidget) CantCarryWidget->SetVisibility(false);

	MyCharacter->SetCanInteract(true);
	MyCharacter->SetActivityState(EPlayerActivityState::FreeRoaming);
}

void AMovieBox::RemoveInteractBinding()
{
	if (!PlayerController || !PlayerController->InputComponent)
	{
		return;
	}

	PlayerController->InputComponent->RemoveActionBinding("Exit Interaction", IE_Pressed);
	PlayerController->InputComponent->RemoveActionBinding(InteractActionName, IE_Pressed);
}
