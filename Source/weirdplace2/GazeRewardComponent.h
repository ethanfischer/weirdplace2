// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GazeRewardComponent.generated.h"

class UAudioComponent;
class UCameraComponent;
class UItemDefinition;
class UMaterialInterface;
class UMaterialInstanceDynamic;
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

	// Screen-space post-process that swells on the player's view as the gaze
	// builds (default-loads M_GazeReward if unset). Leave empty for no effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward|Visual")
	UMaterialInterface* GazeEffectMaterial = nullptr;

	// Peak blendable weight of the effect at full gaze.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward|Visual")
	float MaxEffectWeight = 0.8f;

	// Curve on the visual ramp. ~1.5 builds gently from the start so the player
	// notices something happening.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward|Visual")
	float VisualRampExponent = 1.5f;

	// Curve on the hum ramp. Higher = stays near-silent early then swells late
	// (avoids the sound being audible too soon).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward|Audio")
	float AudioRampExponent = 3.0f;

	// Gradually zoom the player camera in as the gaze builds (degrees subtracted
	// from the base FOV at full gaze). Uses VisualRampExponent for the curve.
	// Skipped entirely in VR — FOV changes are nauseating and the headset owns FOV.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward|Visual")
	bool bEnableFOVZoom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Reward|Visual")
	float MaxFOVZoom = 15.0f;

	// Test-only accessors.
	float GetGazeSeconds() const { return GazeSeconds; }
	UAudioComponent* GetHumComponent() const { return HumComponent; }
	bool WasGranted() const { return bRewardGranted; }
	float GetCurrentEffectWeight() const { return CurrentEffectWeight; }
	float GetCurrentFOV() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool IsPlayerGazingAtOwner() const;
	void GrantReward();
	void ApplyEffectWeight(float Weight);
	void ApplyFOVZoom(float Progress);
	void ResetFOV();

	UPROPERTY()
	UAudioComponent* HumComponent = nullptr;

	UPROPERTY()
	UCameraComponent* CachedPlayerCamera = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* GazeEffectMID = nullptr;

	float GazeSeconds = 0.f;
	float CurrentEffectWeight = 0.f;
	bool bRewardGranted = false;

	// FOV zoom state.
	bool bVRChecked = false;
	bool bIsVR = false;
	bool bBaseFOVCached = false;
	float BaseFOV = 90.f;
};
