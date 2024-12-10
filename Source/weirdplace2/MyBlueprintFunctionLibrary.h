// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyBlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WEIRDPLACE2_API UMyBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	static FRotator GetSpawnedActorRotation(const FRotator& Rotator, int BookcaseIndex);
	UFUNCTION(BlueprintCallable, Category="Actor", meta=(WorldContext="WorldContextObject", DeterminesOutputType="ActorClass"))
	static void SpawnMultiple(const UObject* WorldContextObject, const AActor* SpawnerObject, TSubclassOf<AActor> ActorClass, const TArray<FVector>& ShelfLocations, const TArray<FVector>&
	                          BookcaseLocations, const int AmountPerShelf, const int Spacing, const FVector& SpawnDirection, const UDataTable* DataTable);
};
