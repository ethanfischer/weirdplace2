// Fill out your copyright notice in the Description page of Project Settings.


#include "MovieBox.h"
#include "Kismet/GameplayStatics.h"

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

	InteractionWidget = Cast<UWidgetComponent>(GetDefaultSubobjectByName(TEXT("InteractionText")));
	if (!InteractionWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("Interaction Widget component not found!"));
		return;
	}
	
	EnvelopeMesh = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("Envelope")));
	if (!EnvelopeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("Envelope Mesh component not found!"));
		return;
	}
	
	MyCharacter = Cast<AMyCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!MyCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("MyCharacter not found!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Interaction Widget found successfully!"));

	// Hide it initially
	InteractionWidget->SetVisibility(false);
}

// Called every frame
void AMovieBox::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AMovieBox::InteractWithObject(AActor* Actor, float inspectionDistance)
{
	if (!Actor)
		return;

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

	// Store the actor’s original transform before moving it
	OriginalActorTransform = Actor->GetActorTransform();

	// Offset distance in front of the camera
	FVector NewLocation = CameraLocation + (CameraRotation.Vector() * inspectionDistance);

	// Calculate rotation so the actor's X-axis (forward vector) faces the camera
	FRotator NewRotation = (CameraLocation - NewLocation).Rotation();

	// Set the actor's new position and rotation
	Actor->SetActorLocation(NewLocation);
	Actor->SetActorRotation(NewRotation);
	Actor->SetActorHiddenInGame(false);

	// Store reference to inspected actor
	InspectedActor = Actor;

	// Freeze player camera and movement
	PlayerController->SetIgnoreLookInput(true);
	PlayerController->SetIgnoreMoveInput(true);

	MyCharacter->SetCanInteract(false);

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
}

void AMovieBox::CollectInspectedSubitem()
{
	EnvelopeMesh->SetHiddenInGame(true);
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::Printf(TEXT("Collected subitem")));
}

void AMovieBox::RotateInspectedActor(float AxisValue)
{
	if (!InspectedActor)
		return;
	double DotProduct = FVector::DotProduct(CameraRotation.Vector(), InspectedActor->GetActorForwardVector());
	//Use dot product to determine if movie box and player camera are facing the same world direction
	if (DotProduct > 0.9f)
	{
		//If back of movie box is facing player, try showing collectable UI text on screen
		//prints to screen from cpp like bp
		InteractionWidget->SetVisibility(true);

		PlayerController->InputComponent->BindAction("Collect Inspected Subitem", IE_Pressed, this, &AMovieBox::CollectInspectedSubitem);
	}
	else
	{
		InteractionWidget->SetVisibility(false);
		PlayerController->InputComponent->RemoveActionBinding("Collect Inspected Subitem", IE_Pressed);
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

	// Clear inspected actor reference
	InspectedActor = nullptr;

	MyCharacter->SetCanInteract(true);
}
