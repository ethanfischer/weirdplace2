#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StormBeatController.generated.h"

class AAmbientSound;
enum class EStoryFlag : uint8;

// Environmental half of the tornado-warning beat. A standalone actor the
// designer places once and wires by hand: it fires the moment the store TVs
// switch to the tornado warning (EStoryFlag::TornadoWarningDisplayed) and makes
// the storm close in — the referenced gas-station lights dim (and stay dimmed)
// and the store's TV ambient beds cut out. The per-TV siren/screen lives on
// ACRTTV; this is the room-wide mood. Subscribe/teardown mirrors APayPhone.
UCLASS()
class WEIRDPLACE2_API AStormBeatController : public AActor
{
	GENERATED_BODY()

public:
	AStormBeatController();

	// The exact gas-station light actors to dim (canopy spotlights etc.). Set on
	// the level instance — the designer's spotlights include non-gas-station ones,
	// and some "lights" are emissive meshes with no light component, so there's no
	// safe way to auto-discover them.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Storm|Lights")
	TArray<AActor*> LightsToDim;

	// Each referenced light's intensity is multiplied by this once and left there.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"), Category = "Storm|Lights")
	float DimMultiplier = 0.3f;

	// The store's TV ambient beds (Ambient_TV, Ambient_TV2) to silence when the
	// warning shows. Set on the level instance.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Storm|Audio")
	TArray<AAmbientSound*> AmbientSoundsToSilence;

	// Test hook: configure the controller at runtime (the E2E self-configures one
	// since the placed controller is designer config). Call before the flag fires.
	void ConfigureForTest(const TArray<AActor*>& InLights, const TArray<AAmbientSound*>& InAmbients, float InMultiplier);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnStoryFlagChanged(EStoryFlag Flag, bool bValue);

	// Dim the referenced lights (multiply-once → stays dimmed) and stop the
	// referenced ambient beds. Guarded so it only runs once.
	void ApplyStorm();

	bool bApplied = false;
	FDelegateHandle FlagChangedHandle;
};
