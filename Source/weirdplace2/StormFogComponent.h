#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StormFogComponent.generated.h"

enum class EStoryFlag : uint8;
class UCameraComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Full-screen "pea soup" depth fog that rolls in on the storm beat
// (EStoryFlag::SeenTornadoWarning). Add to the player character. It fades a
// post-process blendable (M_PeaSoupFog) on the camera from clear to fully
// socked-in over RampDuration: everything past FogDistance lerps to a bright
// FogColor, so view distance collapses to a couple of metres REGARDLESS of scene
// lighting — unlike UDS height fog, which just vanishes into a dark night. This
// is the near-field murk; UAltraDynamicWeatherController's "Fog" channel still
// adds the volumetric glow around lights. Mirrors UBladderUrgencyComponent's
// blendable + shader-warmup pattern (a weight-0 blendable is culled and its
// shaders compile on first real use, rendering garbage exactly when seen).
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UStormFogComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStormFogComponent();

	// Seconds for the fog to ramp from clear to fully socked-in once the beat fires.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"), Category = "Storm Fog")
	float RampDuration = 8.f;

	// World distance (cm) at which the scene is fully replaced by fog color. Smaller
	// = thicker soup. ~250 = you can just make out ~2.5 m ahead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0"), Category = "Storm Fog")
	float FogDistance = 250.f;

	// Fog color. Kept bright so the murk reads even in a dark scene (a dark fog color
	// is invisible against a dark background — the whole reason UDS fog failed here).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm Fog")
	FLinearColor FogColor = FLinearColor(0.72f, 0.75f, 0.80f, 1.f);

	// Begin the roll-in now, independent of the story flag (used by the E2E test).
	UFUNCTION(BlueprintCallable, Category = "Storm Fog")
	void StartFog();

	// Dev/testing: snap the fog fully on (if off) or fully off (if on), no ramp. Also
	// keeps the component ticking so live FogDistance/FogColor edits apply immediately.
	UFUNCTION(BlueprintCallable, Category = "Storm Fog")
	void ToggleFog();

	// The current blendable weight (0 = clear, 1 = fully socked in). For tests/debug.
	UFUNCTION(BlueprintPure, Category = "Storm Fog")
	float GetCurrentFogWeight() const { return CurrentWeight; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void OnStoryFlagChanged(EStoryFlag Flag, bool bValue);

	// Drive the fog blendable to Weight (0 = clear, 1 = full soup).
	void SetFogWeight(float Weight);
	void RemoveBlendable();

	UPROPERTY()
	TObjectPtr<UCameraComponent> CachedCamera;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FogMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FogMID;

	bool bRamping = false;
	float RampElapsed = 0.f;
	float CurrentWeight = 0.f;

	FDelegateHandle FlagChangedHandle;
	FTimerHandle WarmupTimerHandle;
};
