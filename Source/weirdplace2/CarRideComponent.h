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

	// Speed profile, driver-style: cruise, ease off to the approach speed
	// before the turn, take the turn at approach speed, creep to a stop.

	// Ease from cruise down to approach speed over this distance (cm), ending
	// where the turn begins
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float DriveBrakingDistance = 10000.0f;

	// Turn-in speed as a fraction of cruise (RideSpeed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float DriveApproachSpeedFraction = 0.3f;

	// The final creep: approach speed eases to a stop over the last this-many cm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float DriveStopDistance = 500.0f;

	// Floor on the creeping speed so the last metres don't crawl forever (cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float DriveMinSpeed = 60.0f;

	// Gas-station lights ramp from 0 to authored intensity over this long after
	// the station is unhidden at drive start (it's ~400m out in the fog then)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float StationLightRampDuration = 6.0f;

	// Sit in the parked car this long before getting out
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Drive")
	float ParkedHoldDuration = 1.2f;

	// --- Getting out: the passenger door opens and the player steps out. No cut. ---

	// The front passenger door, carved off the car mesh (pivot = hinge line)
	UPROPERTY(EditAnywhere, Category = "Car Ride|Door")
	FString PassengerDoorMeshPath = TEXT("/Game/Vehicles/Generated/SM_Car_19c_PassengerDoor.SM_Car_19c_PassengerDoor");

	// Hinge location in car space (where the door mesh's origin sits)
	UPROPERTY(EditAnywhere, Category = "Car Ride|Door")
	FVector DoorHingeLocal = FVector(-101.4159f, 90.936f, 0.0f);

	// Yaw the door swings to when open (negative = outward on this car)
	UPROPERTY(EditAnywhere, Category = "Car Ride|Door")
	float DoorOpenAngle = -80.0f;

	UPROPERTY(EditAnywhere, Category = "Car Ride|Door")
	float DoorOpenDuration = 0.9f;

	// Seat -> standing outside takes this long
	UPROPERTY(EditAnywhere, Category = "Car Ride|Door")
	float ExitDuration = 1.6f;

	// Where the player ends up standing (car space, feet; Z is ignored —
	// capsule sits on the car's ground). Clear of the open door's swing.
	UPROPERTY(EditAnywhere, Category = "Car Ride|Door")
	FVector ExitStandLocal = FVector(-195.0f, 25.0f, 0.0f);

	// Height of the invisible "step out here" volume's centre above the ground
	UPROPERTY(EditAnywhere, Category = "Car Ride|Door")
	float ExitSpotHeight = 70.0f;

	// World-space spot the player stands at after getting out (for tests)
	bool GetExitStandLocation(FVector& OutLocation) const;

	// Station glow meshes: any material scalar with one of these names is
	// treated as an emissive brightness, zeroed at ride start and ramped back up
	// with the lights (the fixtures are emissive meshes, not lights — they'd
	// otherwise show as dots on the horizon while the car sits 400 m out).
	UPROPERTY(EditAnywhere, Category = "Car Ride|Drive")
	TArray<FName> StationEmissiveParams = { FName("amount"), FName("baselineBrightness"), FName("flickerAmount") };

	// Level actors with this tag (e.g. the headlight beam meshes) ride along
	// with the car; Rick always does.
	UPROPERTY(EditAnywhere, Category = "Car Ride|Drive")
	FName RiderActorTag = FName("CarRideAttached");

	// Fade in from black when the ride starts
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float RideStartFadeInDuration = 15.0f;

	// Hold pure black for this long before the ride-start fade-in begins
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float RideStartBlackHoldDuration = 3.0f;

	// Music (actor tagged MusicActorTag) fades in on its own timeline,
	// independent of the black hold / visual+ambient fade. Off = the music
	// actor is silenced with the other ambients and never brought back.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	bool bPlayMusic = false;

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
	// SkipTo stages, in ride order. Shared by the SkipTo cheat
	// (they're spelled "Car<Stage>" there).
	static const TArray<FName>& GetSkipStages();
	static bool IsSkipStage(FName Stage);
	void RebuildScenery();
	AActor* GetSceneryConveyor() const { return SceneryConveyor; }
	// Test hook: end any dialogue in progress and start the drive now
	void ForceEndRide();

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
	void TickExit(float DeltaTime);
	// Player is out: hand control back, Rick steps out, scene cleanup
	void FinishExit();
	void SpawnPassengerDoor();
	void SetDoorAngle(float Yaw);

	// Parked -> (player opens door) -> door swings -> (player clicks the ground)
	// -> steps out. The door stays open.
	enum class EExitPhase : uint8 { None, AwaitDoor, DoorOpening, AwaitStep, Stepping };
	void OnDoorInteract();
	void OnExitSpotInteract();
	EExitPhase ExitPhase = EExitPhase::None;
	float ExitElapsed = 0.0f;
	FVector ExitStartLocation = FVector::ZeroVector;
	FVector ExitEndLocation = FVector::ZeroVector;

	UPROPERTY()
	class ACarDoor* PassengerDoor = nullptr;

	UPROPERTY()
	class ACarExitSpot* ExitSpot = nullptr;

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

	// Actors riding in the car (Rick + RiderActorTag). Posed from a car-local
	// position + rotation delta, never through FTransform relative math — that
	// is lossy for riders with non-uniform scale (the headlight beams are
	// (5,1,1) with a 90° roll and came back smeared across the road).
	UPROPERTY()
	TArray<AActor*> Riders;
	TArray<FVector> RiderLocalLocations;
	TArray<FQuat> RiderLocalRotations;
	FVector PlayerLocalLocation = FVector::ZeroVector;
	bool bPlayerRiding = false;

	// Gas-station lights ramping in over StationLightRampDuration
	UPROPERTY()
	TArray<class ULightComponent*> StationLights;
	TArray<float> StationLightIntensities;
	float LightRampElapsed = 0.0f;
	bool bLightRampActive = false;

	// Station emissive scalars ramping with the lights (parallel arrays)
	UPROPERTY()
	TArray<class UMaterialInstanceDynamic*> StationEmissiveMIDs;
	TArray<FName> StationEmissiveParamNames;
	TArray<float> StationEmissiveValues;

	// Lights + emissives at Alpha (0 = dark, 1 = authored)
	void SetStationGlow(float Alpha);

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
