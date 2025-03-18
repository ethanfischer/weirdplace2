// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "GameFramework/Actor.h"
#include "MovieBox.generated.h"

UCLASS()
class WEIRDPLACE2_API AMovieBox : public AActor, public IInteractable {
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMovieBox();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	virtual void InteractWithObject(AActor* Actor, float inspectionDistance);
	void         RotateInspectedActor(float AxisValue);
	void         StopInspection();

private:
	AActor* InspectedActor;
	FTransform     OriginalActorTransform;
	FRotator CameraRotation;
};
