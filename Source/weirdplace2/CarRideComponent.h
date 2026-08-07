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

	// Seconds of riding after dialogue ends
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float PostDialogueRideTime = 3.0f;

	// Fade to/from black duration in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	float FadeDuration = 1.0f;

	// Empty actor positioned behind windshield where dialogue widget appears
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride")
	AActor* DialogueWidgetTarget;

	// Text color for car ride dialogue (darker for readability against windshield)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car Ride|Settings")
	FSlateColor DialogueTextColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f));

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

	// Material applied to every prop slot so they render as flat dark shapes
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	FString SilhouetteMaterialPath = TEXT("/Game/Materials/M_SolidColor.M_SolidColor");

	// Silhouette color (near-black)
	UPROPERTY(EditAnywhere, Category = "Car Ride|Scenery")
	FLinearColor SilhouetteColor = FLinearColor(0.002f, 0.002f, 0.004f, 1.0f);

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

	float BehindDistance = 0.0f;
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

	bool bSceneryMoving = false;
	bool bBladderPulseArmed = false;

	// Cached widget actor relative transform before car-ride overwrites it
	FVector CachedWidgetRelativeLocation = FVector::ZeroVector;
	FRotator CachedWidgetRelativeRotation = FRotator::ZeroRotator;

	FTimerHandle DialogueStartTimerHandle;
	FTimerHandle PostDialogueTimerHandle;
	FTimerHandle FadeOutTimerHandle;
	FTimerHandle BladderPulseTimerHandle;
};
