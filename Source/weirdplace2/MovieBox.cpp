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

	// Show the collect prompt for the entire inspection if collection is allowed
	const bool bCanCollect = MyCharacter
		&& MyCharacter->GetInventoryComponent()->GetItemCount() < 3
		&& !MyCharacter->IsMovieCollectionLocked();
	InteractionWidget->SetVisibility(bCanCollect);
}

void AMovieBox::CollectInspectedMovie()
{
	if (!InteractionWidget || !EnvelopeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("MovieBox %s: CollectInspectedMovie called with missing components"), *GetName());
		return;
	}
	if (DidCollectMovie) return;

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

	// Add item to inventory with visual data captured from the envelope mesh
	MyCharacter->AddItemToInventoryWithMesh(FName(*CoverName), EnvelopeMesh);

	// Close inspection after collecting
	StopInspection();
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

	// Restore player movement and camera control
	PlayerController->SetIgnoreLookInput(false);
	PlayerController->SetIgnoreMoveInput(false);

	// Unbind input actions. Remove only the axes registered in Interact_Implementation
	// so other systems' axis bindings on the PC stay intact.
	PlayerController->InputComponent->AxisBindings.RemoveAll([](const FInputAxisBinding& Binding)
	{
		return Binding.AxisName == TEXT("Turn Right / Left Mouse")
			|| Binding.AxisName == TEXT("Turn Right / Left Gamepad");
	});
	PlayerController->InputComponent->RemoveActionBinding("Exit Interaction", IE_Pressed);
	PlayerController->InputComponent->RemoveActionBinding("Collect Inspected Movie", IE_Pressed);

	// Hide the collect prompt
	if (InteractionWidget) InteractionWidget->SetVisibility(false);

	// Clear inspected actor reference
	InspectedActor = nullptr;

	GetWorldTimerManager().ClearTimer(CantCarryTimerHandle);
	if (CantCarryWidget) CantCarryWidget->SetVisibility(false);

	MyCharacter->SetCanInteract(true);
	MyCharacter->SetActivityState(EPlayerActivityState::FreeRoaming);
}

