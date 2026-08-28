#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Styling/SlateColor.h"
#include "CarRideComponent.generated.h"

class ARick;
class UBladderUrgencyComponent;
class USceneComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEIRDPLACE2_API UCarRideComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCarRideComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Level instance references ---

	// The driver NPC
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	ARick* Rick;

	// Empty actor at passenger seat position (player teleports here)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	AActor* PassengerSeatTarget;

	// Empty actor at store entrance (player teleports here when ride ends)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	AActor* ArrivalTarget;

	// GasStation folder actor — hidden during ride, shown when ride ends
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	AActor* GasStationRoot;

	// --- Settings ---

	// Seconds before dialogue begins
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float DialogueStartDelay = 3.0f;

	// Fade to/from black duration in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float FadeDuration = 1.0f;

	// Empty actor positioned behind windshield where dialogue widget appears
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	AActor* DialogueWidgetTarget;

	// Text color for car ride dialogue (darker for readability against windshield)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	FSlateColor DialogueTextColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f));

	// How long Rick glances at the player per glance during the ride
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float GlanceAtPlayerDuration = 2.5f;

	// Randomized time Rick looks back at the road between glances
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float GlanceAtRoadDurationMin = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float GlanceAtRoadDurationMax = 8.0f;

	// --- Scenery (runtime-spawned silhouette conveyor; no level assignment needed) ---

	// Apparent car speed in cm/s (scenery moves backward at this rate)
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float RideSpeed = 1500.0f;

	// Total conveyor loop length along the travel axis, in cm.
	// Sparseness target: ~1 prop passing per 5s -> one prop per
	// RideSpeed*5 cm of loop across both sides (6 props / 45000 at 1500).
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float LoopLength = 45000.0f;

	// Fraction of the loop spawned ahead of the car (rest is behind)
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float ForwardBias = 0.65f;

	// Silhouette props scattered per side of the road
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	int32 PropsPerSide = 3;

	// Lateral distance range from the seat's travel axis, in cm
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float PropMinLateral = 1200.0f;

	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float PropMaxLateral = 2700.0f;

	// Uniform scale jitter range for props
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float PropMinScale = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float PropMaxScale = 3.0f;

	// Z offset from the passenger seat target down to the ground plane, in cm
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float GroundZOffset = -150.0f;

	// Deterministic layout seed (E2E relies on a stable layout)
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	int32 RandomSeed = 1337;

	// Meshes used purely for their outlines
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	TArray<FString> PropMeshPaths;

	// Telephone poles: regular rhythm along one side of the road
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	FString PoleMeshPath = TEXT("/Game/Roadside/VOL2/Meshes/LP/SM_Telephone_Pole_01a.SM_Telephone_Pole_01a");

	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float PoleSpacing = 7500.0f;

	// Signed lateral offset from the travel axis (which road side the poles run on)
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float PoleLateral = 800.0f;

	// Props inside this along-axis window (where the headlights point) cast
	// shadows; outside it shadow casting is off to spare the VSM budget.
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float ShadowWindowMinX = -800.0f;

	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	float ShadowWindowMaxX = 3500.0f;

	// Dust motes drifting through the headlight beams
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	int32 DustMoteCount = 12;

	// Test/iteration hooks
	void ForceStartRide();
	void RebuildScenery();
	AActor* GetSceneryConveyor() const { return SceneryConveyor; }
	void ForceEndRide() { EndRide(); }

private:
	void SpawnScenery();
	void DestroyScenery();
	void TickScenery(float DeltaTime);

	UPROPERTY()
	AActor* SceneryConveyor = nullptr;

	UPROPERTY()
	TArray<USceneComponent*> ConveyorItems;

	// Dust motes recycle on their own short loop inside the beam zone
	UPROPERTY()
	TArray<USceneComponent*> MoteItems;

	float BehindDistance = 0.0f;

	// Owner transform at BeginPlay. SkipRide's Rick->AppearOutside() parks the
	// car (and its attached seat target) at the gas station; ForceStartRide
	// restores this so a forced ride runs at the real staging spot.
	FTransform InitialCarTransform;
	void StartRide();
	void SkipRide();
	void StartDialogue();
	void EndRide();
	void OnFadeOutComplete();

	UFUNCTION()
	void OnDialogueEnded();

	UFUNCTION()
	void OnDialogueLineShown(int32 LineIndex);

	void OnBladderPulseFinished();

	// Rick gaze: stare at the road until his first line, then alternate
	// glances at the player with looks back at the road.
	class ULookAtPlayerComponent* GetRickLookAtComponent() const;
	void GlanceAtPlayer();
	void GlanceAtRoad();

	bool bSceneryMoving = false;
	bool bBladderPulseArmed = false;
	bool bGlanceCycleStarted = false;

	// Cached widget actor relative transform before car-ride overwrites it
	FVector CachedWidgetRelativeLocation = FVector::ZeroVector;
	FRotator CachedWidgetRelativeRotation = FRotator::ZeroRotator;

	FTimerHandle DialogueStartTimerHandle;
	FTimerHandle PostDialogueTimerHandle;
	FTimerHandle FadeOutTimerHandle;
	FTimerHandle BladderPulseTimerHandle;
	FTimerHandle GlanceTimerHandle;
};
