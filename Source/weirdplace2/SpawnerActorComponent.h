// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpawnerActorComponent.generated.h"

class AAmbientSound;
class AMovieBox;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEIRDPLACE2_API USpawnerActorComponent : public UActorComponent {
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	USpawnerActorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings")
	TSubclassOf<AActor> MovieBoxClass;

	// Replacement class for the randomly chosen top-shelf box (the "blank tape").
	// Must be a BP_MovieBox subclass with ItemIDOverride set (e.g. BP_BlankVHS).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner Settings")
	TSubclassOf<AMovieBox> BlankVhsBoxClass;
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

	UPROPERTY()
	TArray<AActor*> TopShelfMovieBoxes;

	FName ChosenItemID;

	float CurrentVolumeMultiplier = 1.0f;

	// Last-frame gaze trace result, recorded for diagnostics + the E2E gaze
	// sweep. The chord volume is spotty across the box surface and we want
	// objective data on exactly what the camera-forward trace hits per point.
	bool    bLastHadHit = false;
	bool    bLastLookingAtChosen = false;
	FString LastHitActorName;
	FString LastHitComponentName;
	float   LastHitDistance = -1.0f;
	FVector LastHitImpactPoint = FVector::ZeroVector;

	void SpawnMovieBoxes();

public:
	// Swap a random top-shelf MovieBox for BlankVhsBoxClass and start the chord hum.
	// Called by ASeneca when the "find a blank tape" beat begins.
	void ActivateChosenTape();

	AActor* GetChosenBox() const { return ChosenBox; }
	FName   GetChosenItemID() const { return ChosenItemID; }

	// Diagnostics: last-frame gaze trace state (for the E2E gaze-sweep test).
	void GetGazeDebugState(bool& bOutHasChosen, bool& bOutLooking, bool& bOutHadHit,
		FString& OutHitActor, FString& OutHitComponent, float& OutHitDistance,
		FVector& OutImpactPoint, float& OutVolume) const
	{
		bOutHasChosen = ChosenBox != nullptr;
		bOutLooking = bLastLookingAtChosen;
		bOutHadHit = bLastHadHit;
		OutHitActor = LastHitActorName;
		OutHitComponent = LastHitComponentName;
		OutHitDistance = LastHitDistance;
		OutImpactPoint = LastHitImpactPoint;
		OutVolume = CurrentVolumeMultiplier;
	}

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
