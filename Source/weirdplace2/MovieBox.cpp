// Fill out your copyright notice in the Description page of Project Settings.


#include "MovieBox.h"

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

	// Get the player's controller and pawn
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController)
		return;

	APawn* PlayerPawn = PlayerController->GetPawn();
	if (!PlayerPawn)
		return;

	// Get the camera component (assuming it's a first-person character with a camera)
	FVector  CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	// Offset distance in front of the camera
	FVector NewLocation = CameraLocation + (CameraRotation.Vector() * inspectionDistance);

	// Calculate rotation so the actor's X-axis (forward vector) faces the camera
	FRotator NewRotation = (CameraLocation - NewLocation).Rotation();

	// Set the actor's new position and rotation
	Actor->SetActorLocation(NewLocation);
	Actor->SetActorRotation(NewRotation);
	Actor->SetActorHiddenInGame(false);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		                                 -1,
		                                 5.f,
		                                 FColor::Green,
		                                 FString::Printf(TEXT("%s is now in front of the player for inspection"), *Actor->GetName())
		                                );
	}
}
