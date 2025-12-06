// Fill out your copyright notice in the Description page of Project Settings.


#include "TimedVisibility.h"

#include "Kismet/GameplayStatics.h"

// Sets default values for this component's properties
UTimedVisibility::UTimedVisibility()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UTimedVisibility::BeginPlay()
{
	Super::BeginPlay();
	Owner = GetOwner();
	World = GetWorld();
	PlayerController = UGameplayStatics::GetPlayerController(World, 0);

	if (ToggleFrequencyInSecondsRandomRange != FVector2D(0, 0)) //default value
	{
		ToggleFrequencyInSeconds = FMath::FRandRange(ToggleFrequencyInSecondsRandomRange.X, ToggleFrequencyInSecondsRandomRange.Y);
	}
}

bool UTimedVisibility::IsInView()
{
	if (CanToggleWhileInView)
	{
		return false;
	}

	FVector  PlayerViewLocation;
	FRotator PlayerViewRotation;

	if (PlayerController)
	{
		PlayerController->GetPlayerViewPoint(PlayerViewLocation, PlayerViewRotation);

		FVector ForwardVector = PlayerViewRotation.Vector();
		FVector ActorLocation = Owner->GetActorLocation();
		FVector DirectionToActor = (ActorLocation - PlayerViewLocation).GetSafeNormal();

		float DotProduct = FVector::DotProduct(ForwardVector, DirectionToActor);
		float ViewAngleCos = FMath::Cos(FMath::DegreesToRadians(180.0f / 2.0f));

		return DotProduct > ViewAngleCos;
	}

	return false;
}


// Called every frame
void UTimedVisibility::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Timer += DeltaTime;

	if (Timer > ToggleFrequencyInSeconds)
	{
		ShouldToggleVisibility = true;
		Timer = 0;
	}

	if (ShouldToggleVisibility && !IsInView())
	{
		IsVisible = !IsVisible;
		Owner->SetActorHiddenInGame(IsVisible); // Hides the actor
		Owner->SetActorEnableCollision(!IsVisible); // Disables collision
		Owner->SetActorTickEnabled(IsVisible); // Stops ticking to improve performance
		ShouldToggleVisibility = false;
	}
}
