// Fill out your copyright notice in the Description page of Project Settings.


#include "MyBlueprintFunctionLibrary.h"

void UMyBlueprintFunctionLibrary::SpawnMultiple(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass)
{
	for (int i = 0; i < 10; i++)
	{
		AActor* Actor = WorldContextObject->GetWorld()->SpawnActor<AActor>(ActorClass, FVector::ZeroVector, FRotator::ZeroRotator);
	}
}
