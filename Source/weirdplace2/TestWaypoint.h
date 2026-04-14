#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestWaypoint.generated.h"

class UBillboardComponent;

// Lightweight marker actor used by the E2E test driver as a teleport target.
// Placed in levels and referenced by Tag. Pose (location + rotation) is used
// when teleporting the player: actor rotation becomes the player's view.
UCLASS()
class WEIRDPLACE2_API ATestWaypoint : public AActor
{
	GENERATED_BODY()

public:
	ATestWaypoint();

	// Unique tag used by tests to find this waypoint.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Test Waypoint")
	FName Tag;

	// Returns the first ATestWaypoint in World with a matching Tag, or nullptr.
	static ATestWaypoint* FindByTag(const UObject* WorldContext, FName InTag);

private:
	UPROPERTY(VisibleAnywhere, Category = "Test Waypoint")
	UBillboardComponent* Billboard;
};
