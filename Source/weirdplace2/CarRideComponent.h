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

	// --- Ride end: the car really drives to the gas station and pulls over ---

	// Radius of the quarter-turn into Rick's parking spot (cm). The approach
	// lane is laid out so a straight run + this turn lands exactly on the spot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float DriveTurnRadius = 1500.0f;

	// Constant-deceleration braking starts this far (cm) before the turn begins
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float DriveBrakingDistance = 3000.0f;

	// Floor on the braking speed so the last metres don't crawl forever (cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float DriveMinSpeed = 150.0f;

	// Gas-station lights ramp from 0 to authored intensity over this long after
	// the station is unhidden at drive start (it's ~400m out in the fog then)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float StationLightRampDuration = 6.0f;

	// Sit in the parked car this long before getting out
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float ParkedHoldDuration = 1.2f;

	// Getting out of the car is a quick dip to black (not the old long fade)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float ExitDipOutDuration = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float ExitDipInDuration = 0.8f;

	// Level actors with this tag (e.g. the headlight beam meshes) ride along
	// with the car; Rick always does.
	UPROPERTY(EditAnywhere, Category = "Car Ride|Drive")
	FName RiderActorTag = FName("CarRideAttached");

	// Fade in from black when the ride starts
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float RideStartFadeInDuration = 30.0f;

	// Hold pure black for this long before the ride-start fade-in begins
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float RideStartBlackHoldDuration = 3.0f;

	// Music (actor tagged MusicActorTag) fades in on its own timeline,
	// independent of the black hold / visual+ambient fade
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float MusicFadeInDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float MusicFadeInDuration = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	FName MusicActorTag = FName("CarRideMusic");

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

	// Dialogue only advances (and shows the dialogue reticle) while the player
	// looks within this cone of the windshield direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float DialogueGazeMaxAngleDeg = 35.0f;

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
	// Dev: jump the running ride to a stage, dropping the start fade and any
	// dialogue in progress. Stages: "Dialogue" (Rick's first line, screen
	// clear), "LastLine" (Rick's final line showing; advance it yourself)
	// or "Drive" (dialogue done, car pulling away). Console:
	// weird.CarRide.SkipTo <Stage>
	void SkipTo(FName Stage);
	void RebuildScenery();
	AActor* GetSceneryConveyor() const { return SceneryConveyor; }
	void ForceEndRide() { EndRide(); }

private:
	void SpawnScenery();
	void DestroyScenery();
	// Advance the conveyor by Step cm (props slide backward, recycle forward)
	void TickScenery(float Step);
	// Hand the props over to the world (they stop recycling and stay put) —
	// done when the car starts its turn so they don't swing with it
	void FreezeSceneryProps();

	// --- Drive (ride end) ---
	void BeginDrive();
	void TickDrive(float DeltaTime);
	// Position + unit tangent (XY) at distance S along the approach path
	void EvaluateDrivePath(float S, FVector& OutLocation, FVector2D& OutTangent) const;
	// Place the car and re-pose everything riding in it
	void PlaceCar(const FTransform& CarTransform);
	void OnCarParked();
	void ExitCar();
	void OnExitDipComplete();

	bool bDriving = false;
	float DriveS = 0.0f;              // distance travelled along the path (cm)
	float DriveStraightLength = 0.0f; // Start -> P0
	float DriveCurveLength = 0.0f;    // P0 -> P2 (quadratic bezier via P1)
	FVector2D DriveStart = FVector2D::ZeroVector;
	FVector2D DriveDir = FVector2D::ZeroVector; // approach direction (unit)
	FVector2D DriveP0 = FVector2D::ZeroVector;
	FVector2D DriveP1 = FVector2D::ZeroVector;
	FVector2D DriveP2 = FVector2D::ZeroVector;
	float DriveStartZ = 0.0f;
	float DriveEndZ = 0.0f;
	float CarYawFromTangent = 0.0f;   // car yaw minus travel-direction yaw
	TArray<float> DriveCurveArcTable;  // cumulative length at uniform t samples
	// Recycled props that would land past this path distance are hidden —
	// they'd otherwise stand in the gas station forecourt
	float DriveSceneryEndS = 0.0f;
	bool bSceneryFrozen = false;

	// Actors riding in the car (Rick + RiderActorTag), posed relative to it
	UPROPERTY()
	TArray<AActor*> Riders;
	TArray<FTransform> RiderRelativeTransforms;
	FTransform PlayerRelativeTransform;
	bool bPlayerRiding = false;

	// Gas-station lights ramping in over StationLightRampDuration
	UPROPERTY()
	TArray<class ULightComponent*> StationLights;
	TArray<float> StationLightIntensities;
	float LightRampElapsed = 0.0f;
	bool bLightRampActive = false;

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
	bool bDialogueStarted = false;

	// Lift the ride-start black (camera fade + ambient fade-in). Runs on the
	// black-hold timer, or at once from SkipTo.
	void RevealRideStart(float FadeSeconds);

	// Cached widget actor relative transform before car-ride overwrites it
	FVector CachedWidgetRelativeLocation = FVector::ZeroVector;
	FRotator CachedWidgetRelativeRotation = FRotator::ZeroRotator;

	FTimerHandle DialogueStartTimerHandle;
	FTimerHandle PostDialogueTimerHandle;
	FTimerHandle ParkedHoldTimerHandle;
	FTimerHandle ExitDipTimerHandle;
	FTimerHandle BladderPulseTimerHandle;
	FTimerHandle GlanceTimerHandle;
	FTimerHandle RideStartFadeTimerHandle;
	FTimerHandle MusicFadeTimerHandle;

	// Ambient/music components silenced at ride start, with their original
	// volume multipliers to fade back up to
	UPROPERTY()
	TArray<class UAudioComponent*> SilencedAmbients;
	TArray<float> SilencedAmbientVolumes;

	UPROPERTY()
	class UAudioComponent* MusicComponent = nullptr;
	float MusicOriginalVolume = 1.0f;

	void SilenceWorldAudioForRideStart();
};
