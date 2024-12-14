// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimedVisibility.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEIRDPLACE2_API UTimedVisibility : public UActorComponent {
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTimedVisibility();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timed Visibility Settings")
	float        ToggleFrequencyInSeconds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timed Visibility Settings")
	bool         CanToggleWhileInView;
	
private:
	AActor*      Owner;
	UWorld*      World;
	bool         ShouldToggleVisibility;
	bool         IsVisible = true;
	APlayerController* PlayerController;
	float        Timer;

public:
	bool IsInView();
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
