// Fill out your copyright notice in the Description page of Project Settings.


#include "MyBlueprintFunctionLibrary.h"

void UMyBlueprintFunctionLibrary::SpawnMultiple(const UObject*         WorldContextObject, const AActor* SpawnerObject, TSubclassOf<AActor> ActorClass,
                                                const TArray<FVector>& ShelfLocations,
                                                const TArray<FVector>& BookcaseLocations, const int AmountPerShelf, const int Spacing, const FVector& SpawnDirection)
{
	const auto SpawnerLocation = SpawnerObject->GetActorLocation();
	const auto SpawnerRotation = SpawnerObject->GetActorRotation();
	
	for (auto bookcaseIndex = 0; bookcaseIndex < BookcaseLocations.Num(); bookcaseIndex++)
	{
		for (auto shelfIndex = 0; shelfIndex < ShelfLocations.Num(); shelfIndex++)
		{
			for (auto i = 0; i < AmountPerShelf; i++)
			{
				const auto SpawnedActor = WorldContextObject->GetWorld()->SpawnActor<AActor>(ActorClass, SpawnerLocation, SpawnerRotation);

				auto AdjustedLocation = SpawnerLocation + (i * Spacing * SpawnDirection + (BookcaseLocations[bookcaseIndex] + ShelfLocations[shelfIndex]));
				SpawnedActor->SetActorLocation(AdjustedLocation);
			}
		}
	}
}
