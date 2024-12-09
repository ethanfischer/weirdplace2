// Fill out your copyright notice in the Description page of Project Settings.


#include "MyBlueprintFunctionLibrary.h"

void UMyBlueprintFunctionLibrary::SpawnMultiple(const UObject*         WorldContextObject, const AActor* SpawnerObject, TSubclassOf<AActor> ActorClass, const TArray<FVector>& ShelfLocations,
                                                const TArray<FVector>& BookcaseLocations, const int AmountPerShelf, const int Spacing, FVector& SpawnDirection)
{
	for (int bookcaseIndex = 0; bookcaseIndex < BookcaseLocations.Num(); bookcaseIndex++)
	{
		for (int shelfIndex = 0; shelfIndex < ShelfLocations.Num(); shelfIndex++)
		{
			for(int i = 0; i < AmountPerShelf; i++)
			{
				//Get actor position
				const FVector& SpawnLocation = SpawnerObject->GetActorLocation();
				
				AActor* Spawned = WorldContextObject->GetWorld()->SpawnActor<AActor>(ActorClass, SpawnLocation, FRotator::ZeroRotator);
				int     a = i * Spacing;
				FVector b = a * SpawnDirection;
				FVector     AdjustedSpawnLocation = b + (BookcaseLocations[bookcaseIndex] + ShelfLocations[shelfIndex]);
				
				Spawned->SetActorLocation(AdjustedSpawnLocation);
			}
		}
	}
}
