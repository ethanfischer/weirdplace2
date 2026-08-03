#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StormFogComponent.generated.h"

enum class EStoryFlag : uint8;
class UCameraComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Full-screen "pea soup" depth fog that rolls in on the storm beat
// (EStoryFlag::SeenTornadoWarning). Add to the player character. It drives a
// post-process blendable (M_PeaSoupFog) on the camera whose fog distance CLOSES
// IN over RampDuration — from FogStartDistance (near-clear) down to FogDistance
// (a couple of metres) — so the murk creeps toward the player like real fog
// rolling in, instead of the whole view crossfading to fog at once. Everything
// past the current distance lerps to FogColor, REGARDLESS of scene lighting
// (unlike UDS height fog, which vanishes into a dark night). This is the
// near-field murk; UltraDynamicWeatherController's "Fog" channel adds the
// volumetric glow around lights. Mirrors UBladderUrgencyComponent's blendable +
// shader-warmup pattern (a weight-0 blendable is culled and its shaders compile
// on first real use, rendering garbage exactly when seen).
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UStormFogComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStormFogComponent();

	// Seconds for the fog to ramp from clear to fully socked-in once the beat fires.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"), Category = "Storm Fog")
	float RampDuration = 8.f;

	// SETTLED fog distance (cm) — where the murk ends up after rolling in. Smaller =
	// thicker soup. ~250 = you can just make out ~2.5 m ahead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0"), Category = "Storm Fog")
	float FogDistance = 250.f;

	// STARTING fog distance (cm) — where the murk begins before it closes in. Should be
	// well beyond the visible scene so the roll-in starts near-clear; the distance then
	// eases geometrically from here down to FogDistance over RampDuration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0"), Category = "Storm Fog")
	float FogStartDistance = 20000.f;

	// Fog color the scene fades toward past FogDistance. Black = distance dissolves to
	// a void (going-blind horror). A post-process fog stays visible even when black
	// (unlike UDS height fog), because it hard-replaces distant pixels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm Fog")
	FLinearColor FogColor = FLinearColor(0.f, 0.f, 0.f, 1.f);

	// Caps how opaque the fog gets. 1.0 = fully hides everything past FogDistance;
	// 0.7 = tops out at 70% so 30% of the scene always bleeds through (thinner, hazier).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"), Category = "Storm Fog")
	float FogMaxOpacity = 1.f;

	// Begin the roll-in now, independent of the story flag (used by the E2E test).
	UFUNCTION(BlueprintCallable, Category = "Storm Fog")
	void StartFog();

	// Dev/testing: snap the fog fully on (if off) or fully off (if on), no ramp. Also
	// keeps the component ticking so live FogDistance/FogColor edits apply immediately.
	UFUNCTION(BlueprintCallable, Category = "Storm Fog")
	void ToggleFog();

	// Roll-in amount: 0 = clear, 1 = fully closed in (murk at FogDistance). For tests/debug.
	UFUNCTION(BlueprintPure, Category = "Storm Fog")
	float GetFogAmount() const { return FogAmount; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void OnStoryFlagChanged(EStoryFlag Flag, bool bValue);

	// Ease the roll-in amount from its current value to Target (0 clear, 1 settled) over
	// Duration seconds (0 = instant). The visible motion is the fog distance closing in.
	void BeginRamp(float Target, float Duration);

	// Apply a roll-in amount: eased geometric interp of the fog distance from
	// FogStartDistance (Amount 0) to FogDistance (Amount 1), push params, and drive the
	// blendable (weight 0 when clear, 1 once engaged — the distance does the fading).
	void ApplyFog(float Amount);

	// Low-level: set the fog blendable's weight on the camera post-process.
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
	float FogAmount = 0.f;     // 0 = clear, 1 = fully closed in
	float RampFromAmount = 0.f;
	float RampToAmount = 1.f;
	float ActiveRampDuration = 0.f;

	FDelegateHandle FlagChangedHandle;
	FTimerHandle WarmupTimerHandle;
};
