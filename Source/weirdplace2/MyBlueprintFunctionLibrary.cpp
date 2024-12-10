// Fill out your copyright notice in the Description page of Project Settings.


#include "MyBlueprintFunctionLibrary.h"

void UMyBlueprintFunctionLibrary::SpawnMultiple(const UObject*         WorldContextObject, const AActor* SpawnerObject, TSubclassOf<AActor> ActorClass,
                                                const TArray<FVector>& ShelfLocations,
                                                const TArray<FVector>& BookcaseLocations, const int AmountPerShelf, const int Spacing, const FVector& SpawnDirection,
                                                const UDataTable*      DataTable)
{
	const auto SpawnerLocation = SpawnerObject->GetActorLocation();
	const auto SpawnerRotation = SpawnerObject->GetActorRotation();
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
				WorldContextObject->GetWorld()->SpawnActor<AActor>(ActorClass, AdjustedLocation, AdjustedRotation, *SpawnParameters);
				VideoNameIndex++;
			}
		}
	}
}

FRotator UMyBlueprintFunctionLibrary::GetSpawnedActorRotation(const FRotator& Rotator, const int bookcaseIndex)
{
	if (bookcaseIndex % 2 == 0)
	{
		return FRotator(Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
	}

	return FRotator(Rotator.Pitch, Rotator.Yaw + 180, Rotator.Roll);
}
