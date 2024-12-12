// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnerActorComponent.h"

// Sets default values for this component's properties
USpawnerActorComponent::USpawnerActorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}

void USpawnerActorComponent::BeginPlay()
{
	Super::BeginPlay();

	Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("USpawnManagerComponent::SpawnMultiple - No Owner Found"));
	}

	World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("USpawnManagerComponent::SpawnMultiple - No World Found"));
	}

	SpawnMovieBoxes();
}

void USpawnerActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USpawnerActorComponent::SpawnMovieBoxes()
{
	const auto SpawnerLocation = Owner->GetActorLocation();
	const auto SpawnerRotation = Owner->GetActorRotation();
	const auto SpawnerRotationFlipped = FRotator(SpawnerRotation.Pitch, SpawnerRotation.Yaw + 180, SpawnerRotation.Roll);
	const auto VideoNames = DataTable->GetRowNames();
	auto       VideoNameIndex = 0;

	for (auto ShelfIndex = 0; ShelfIndex < ShelfLocations.Num(); ShelfIndex++)
	{
		for (auto BookcaseIndex = 0; BookcaseIndex < BookcaseLocations.Num(); BookcaseIndex++)
		{
			for (auto i = 0; i < AmountPerShelf; i++)
			{
				auto AdjustedLocation = SpawnerLocation + (i * Spacing * SpawnDirection + (BookcaseLocations[BookcaseIndex] + ShelfLocations[ShelfIndex]));
				auto AdjustedRotation = BookcaseIndex % 2 == 0 ? SpawnerRotation : SpawnerRotationFlipped;
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = VideoNames[VideoNameIndex];
				World->SpawnActor<AActor>(MovieBoxClass, AdjustedLocation, AdjustedRotation, SpawnParameters);
				VideoNameIndex++;
			}
		}
	}
}