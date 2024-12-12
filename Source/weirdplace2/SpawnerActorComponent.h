// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpawnerActorComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEIRDPLACE2_API USpawnerActorComponent : public UActorComponent {
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	USpawnerActorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings")
	TSubclassOf<AActor> MovieBoxClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings")
	TArray<FVector> ShelfLocations;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings")
	TArray<FVector> BookcaseLocations;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings")
	int32 AmountPerShelf = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings")
	int32 Spacing = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings")
	FVector SpawnDirection = FVector::ForwardVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings")
	UDataTable* DataTable;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	AActor* Owner;
	UWorld* World;

	void SpawnMovieBoxes();

	static FRotator GetSpawnedActorRotation(const FRotator& Rotator, const int bookcaseIndex);

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
