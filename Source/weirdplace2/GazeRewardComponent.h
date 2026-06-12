// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GazeRewardComponent.generated.h"

class UAudioComponent;
class UItemDefinition;
class USoundBase;

// Drop this on any actor: stare at that actor long enough and it grants the
// editor-supplied RewardItem to the player's inventory. Gaze counts when the
// camera's center ray crosses the owner's bounding box — any point of it,
// which matters for long fixtures like the gas-station canopy bar. An optional
// hum swells as the held gaze approaches RequiredLookSeconds; looking away (or,
// if bRequireLineOfSight, anything occluding the owner) resets both the timer
// and the hum. Grants once per game when bOneShot.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UGazeRewardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGazeRewardComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Item granted when the stare completes (drag an Item Definition asset here).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward")
	UItemDefinition* RewardItem = nullptr;

	// Continuous look time required, in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward")
	float RequiredLookSeconds = 30.f;

	// Beyond this camera distance the gaze never counts, in cm.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward")
	float MaxGazeDistance = 6000.f;

	// Optional sound that swells while the gaze is held (leave empty for none).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward")
	USoundBase* HumSound = nullptr;

	// Padding (cm) added to the owner's bounds so edges of the fixture still count.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward")
	float BoxExpand = 10.f;

	// Require an unobstructed line of sight to the owner. Disable for owners near
	// UI widgets/text, which block ECC_Visibility and would falsely occlude.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward")
	bool bRequireLineOfSight = true;

	// Grant once, then stop. Off = re-arms after each grant.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward")
	bool bOneShot = true;

	// Test-only accessors.
	float GetGazeSeconds() const { return GazeSeconds; }
	UAudioComponent* GetHumComponent() const { return HumComponent; }
	bool WasGranted() const { return bRewardGranted; }

protected:
	virtual void BeginPlay() override;

private:
	bool IsPlayerGazingAtOwner() const;
	void GrantReward();

	UPROPERTY()
	UAudioComponent* HumComponent = nullptr;

	float GazeSeconds = 0.f;
	bool bRewardGranted = false;
};
