#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GazeUtils.generated.h"

// Shared gaze geometry: is the player looking at an actor? Used by both the
// gaze-reward fixtures and the tornado-warning TV watch.
UCLASS()
class WEIRDPLACE2_API UGazeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// True when the player camera's center ray crosses Target's bounding box
	// (expanded by BoxExpand) within MaxDistance. Box test, not a hit test, so
	// it works on fixtures with no collision. With bRequireLineOfSight, anything
	// solid between the camera and the box-entry point blocks the gaze; Target
	// and the player pawn are ignored so the target's own mesh never self-occludes.
	static bool IsActorInPlayerGaze(const AActor* Target, UWorld* World,
		float BoxExpand = 10.f, float MaxDistance = 6000.f, bool bRequireLineOfSight = true);

	// True when Point lies within MinDot of the player camera's forward direction —
	// a simple cone test (no bounds, no line-of-sight), for "looking roughly toward
	// X". MinDot 0.5 ~= a 60-degree half-angle cone.
	static bool IsPointInPlayerView(const FVector& Point, UWorld* World, float MinDot = 0.5f);
};
