#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UltraDynamicWeatherController.generated.h"

enum class EStoryFlag : uint8;
class AAmbientSound;

// Eases the scene into a storm once the player has seen the tornado warning on the
// store TVs (EStoryFlag::SeenTornadoWarning). Place ONE in the level; it finds the
// Ultra_Dynamic_Sky and Ultra_Dynamic_Weather actors by class automatically and,
// over a single transition, fades the sky's "Overall Intensity" down (gloom), ramps
// the weather's "Wind Intensity" up (gusts), thickens the weather's "Fog" so view
// distance collapses (socked-in murk), and swells the global wind ambient's
// volume. This is the *atmospheric* half of the storm beat; the room-wide
// lights/audio half lives on AStormBeatController. Subscribe/teardown mirrors
// AStormBeatController / APayPhone.
UCLASS()
class WEIRDPLACE2_API AUltraDynamicWeatherController : public AActor
{
	GENERATED_BODY()

public:
	AUltraDynamicWeatherController();

	// How long the whole storm transition takes, in seconds. Smaller = the weather
	// turns faster. Governs BOTH the sky fade and the wind ramp.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", DisplayName = "Transition Duration (Seconds)"), Category = "Storm Weather")
	float FadeDuration = 120.f;

	// Overall Intensity the Ultra_Dynamic_Sky settles at after the key breaks. UDS
	// starts at 2.0; fading to 0.25 dims the whole scene into an overcast gloom.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"), Category = "Storm Weather")
	float TargetOverallIntensity = 0.25f;

	// Wind Intensity at the moment the storm starts. UDW ignores its Wind Intensity
	// value unless "Manual Override" is on (off, it derives wind from the weather
	// state and parks the value at a sentinel) — so the controller flips override on
	// and ramps from this baseline. 100 = roughly the calm-weather feel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", DisplayName = "Start Wind Intensity"), Category = "Storm Weather")
	float StartWindIntensity = 100.f;

	// Wind Intensity the storm ramps up to. Raise for a gustier storm (tune to taste —
	// UDW's wind scale is open-ended).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", DisplayName = "Target Wind Intensity"), Category = "Storm Weather")
	float TargetWindIntensity = 300.f;

	// The "Fog" weather value the scene thickens to as the storm builds. UDS's "Fog"
	// scalar drives overall fogginess (height + volumetric); it's open-ended and does
	// NOT map linearly to density, so tune this by eye. UDW normally owns this value,
	// so the controller flips UDW's "Fog - Manual Override" on and ramps from the
	// current value up to this. ~3 is the calm baseline; higher = view distance
	// collapses toward a few metres.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", DisplayName = "Target Fog"), Category = "Storm Weather")
	float TargetFog = 20.f;

	// The Ambient_GlobalWind ambient sound whose volume swells as the storm builds.
	// Assign on the level INSTANCE — it's a placed AmbientSound, not findable by class
	// (its class is just "AmbientSound"; the "Ambient_GlobalWind" name is an editor
	// label that doesn't exist at runtime).
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Storm Weather")
	TObjectPtr<AAmbientSound> AmbientGlobalWind;

	// Volume multiplier the wind ambient swells to. Ramps from its current volume
	// (typically 1.0) up to this over the transition.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", DisplayName = "Target Wind Volume"), Category = "Storm Weather")
	float TargetWindVolume = 3.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void OnStoryFlagChanged(EStoryFlag Flag, bool bValue);

	// Resolve both UDS actors, capture their current values, and start the per-tick
	// transition. Guarded to run once. Sky and wind are independent channels — if
	// one actor/property is missing it's skipped (with an error) and the other still
	// runs.
	void BeginFade();

	// Apply the interpolated sky/wind values at the given 0..1 transition alpha.
	void ApplyAtAlpha(float Alpha);

	// Locate a placed actor whose class name contains Needle. Null if absent.
	AActor* FindActorByClassNeedle(const TCHAR* Needle) const;

	bool bFading = false;
	float FadeElapsed = 0.f;

	// Sky "Overall Intensity" channel.
	bool bFadeSky = false;
	float SkyStartIntensity = 2.f;

	// Weather "Wind Intensity" channel (ramps StartWindIntensity -> TargetWindIntensity).
	bool bRampWind = false;

	// Weather "Fog" channel (ramps FogStart -> TargetFog). Same manual-override
	// machinery as wind: UDW owns the value unless the override bool is engaged.
	bool bRampFog = false;
	float FogStart = 3.f;

	// Ambient wind "Volume Multiplier" channel (ramps WindVolumeStart -> TargetWindVolume).
	bool bLoudenWind = false;
	float WindVolumeStart = 1.f;

	FDelegateHandle FlagChangedHandle;

	UPROPERTY()
	TWeakObjectPtr<AActor> SkyActor;

	UPROPERTY()
	TWeakObjectPtr<AActor> WeatherActor;
};
