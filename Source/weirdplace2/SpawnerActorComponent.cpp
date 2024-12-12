// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnerActorComponent.h"

// Sets default values for this component's properties
USpawnerActorComponent::USpawnerActorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
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


// Called every frame
void USpawnerActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void USpawnerActorComponent::SpawnMovieBoxes()
{
	const auto SpawnerLocation = Owner->GetActorLocation();
	const auto SpawnerRotation = Owner->GetActorRotation();
	const auto VideoNames = DataTable->GetRowNames();
	auto       VideoNameIndex = 0;

	for (auto ShelfIndex = 0; ShelfIndex < ShelfLocations.Num(); ShelfIndex++)
	{
		for (auto BookcaseIndex = 0; BookcaseIndex < BookcaseLocations.Num(); BookcaseIndex++)
		{
			for (auto i = 0; i < AmountPerShelf; i++)
			{
				auto       AdjustedLocation = SpawnerLocation + (i * Spacing * SpawnDirection + (BookcaseLocations[BookcaseIndex] + ShelfLocations[ShelfIndex]));
				auto       AdjustedRotation = GetSpawnedActorRotation(SpawnerRotation, BookcaseIndex);
				const auto SpawnParameters = new FActorSpawnParameters();
				SpawnParameters->Name = VideoNames[VideoNameIndex];
				World->SpawnActor<AActor>(MovieBoxClass, AdjustedLocation, AdjustedRotation, *SpawnParameters);
				VideoNameIndex++;
			}
		}
	}
}

FRotator USpawnerActorComponent::GetSpawnedActorRotation(const FRotator& Rotator, const int bookcaseIndex)
{
	if (bookcaseIndex % 2 == 0)
	{
		return FRotator(Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
	}

	return FRotator(Rotator.Pitch, Rotator.Yaw + 180, Rotator.Roll);
}
