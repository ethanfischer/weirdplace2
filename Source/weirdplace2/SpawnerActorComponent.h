// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpawnerActorComponent.generated.h"

class AAmbientSound;

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

	// Ambient sound to teleport onto a randomly chosen spawned MovieBox (set on level instance)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Spawner Settings")
	AAmbientSound* ChordAmbientSound = nullptr;

	// Distance (cm) to push the randomly chosen MovieBox along its own forward axis
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings")
	float ChosenForwardOffset = 10.0f;

	// Volume multiplier on ChordAmbientSound when player looks directly at the chosen MovieBox
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings|Look-At Boost")
	float LookAtVolumeMultiplier = 3.0f;

	// Volume fade-in/out speed (multiplier units per second)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings|Look-At Boost")
	float LookAtFadeSpeed = 4.0f;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	AActor* Owner;
	UWorld* World;

	UPROPERTY()
	AActor* ChosenBox = nullptr;

	float CurrentVolumeMultiplier = 1.0f;

	void SpawnMovieBoxes();

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
