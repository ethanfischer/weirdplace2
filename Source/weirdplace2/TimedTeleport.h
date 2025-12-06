// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimedTeleport.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WEIRDPLACE2_API UTimedTeleport : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UTimedTeleport();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TimedTeleport)
	int TimeToTeleport;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TimedTeleport)
	FVector TeleportLocation;
	//Rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TimedTeleport)
	FRotator TeleportRotation;
private:
	float Timer;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
