#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "CarDoor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UBoxComponent;

DECLARE_MULTICAST_DELEGATE(FOnCarRideInteract);

// The car's front passenger door, spawned by UCarRideComponent and attached to
// the car at the hinge. The root IS the hinge: rotate the actor relatively to
// swing it. Interactable only while armed (parked, waiting for the player to
// open it).
UCLASS()
class WEIRDPLACE2_API ACarDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACarDoor();

	void SetMesh(UStaticMesh* Mesh);
	// Armed = interact target; it only has collision while armed so the open
	// door never pushes the stepping-out player around
	void SetArmed(bool bInArmed);
	void SetOpenYaw(float Yaw);

	// IInteractable
	virtual void Interact_Implementation() override;
	virtual bool CanInteract() override { return bArmed; }

	FOnCarRideInteract OnInteract;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;

	// Where tests aim: the middle of the door panel, not the hinge
	UPROPERTY(VisibleAnywhere)
	USceneComponent* AimPoint;

private:
	bool bArmed = false;
};

// Invisible interactable volume on the ground outside the open door: the
// player looks at it and presses interact to get out. Destroyed once used.
UCLASS()
class WEIRDPLACE2_API ACarExitSpot : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACarExitSpot();

	void SetArmed(bool bInArmed) { bArmed = bInArmed; }

	// IInteractable
	virtual void Interact_Implementation() override;
	virtual bool CanInteract() override { return bArmed; }

	FOnCarRideInteract OnInteract;

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* Box;

private:
	bool bArmed = false;
};
