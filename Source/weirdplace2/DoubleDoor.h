#pragma once

#include "CoreMinimal.h"
#include "Door.h"
#include "DoubleDoor.generated.h"

class USkeletalMeshComponent;

// A glass double door (two leaves) that inherits all of ADoor's behavior —
// E-to-interact, toggle, auto-close, swing-away-from-player, lock/keypad — but
// drives a skeletal AnimBP instead of rotating a rigid static mesh. Each open
// step pushes the two leaf-angle floats (LeftDoor_Rotation/RightDoor_Rotation)
// into BP_Anim_DoubleDoors, whose AnimGraph poses the two leaf bones.
UCLASS()
class WEIRDPLACE2_API ADoubleDoor : public ADoor
{
	GENERATED_BODY()

public:
	ADoubleDoor();

	// Picks which single leaf to swing (the one the player is standing in front
	// of), then runs the inherited open/close cycle.
	virtual void Interact_Implementation() override;

	// Signed open amount in degrees (debug/test/Blueprint read).
	UFUNCTION(BlueprintPure, Category = "Door")
	float GetDoorState() const { return DoorState; }

	// Reads back the leaf angles currently pushed into the AnimInstance
	// (LeftDoor_Rotation/RightDoor_Rotation). Returns false if there's no
	// AnimInstance or the float vars are missing. Used by the E2E guard.
	bool GetLeafAngles(float& OutLeft, float& OutRight) const;

protected:
	virtual void ApplyOpenAmount(float Alpha) override;

	// Skeletal root for the rigged double-door mesh (assigned in BP, like
	// BP_Door assigns its static mesh). Replaces the inherited static DoorMesh
	// as RootComponent; DoorMesh is parked under it, hidden and unused.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	USkeletalMeshComponent* DoorSkeletalMesh;

	// Signed open amount in degrees, exposed for debugging/Blueprint reads.
	// (The AnimBP is actually driven by ApplyOpenAmount, which pushes the two
	// LeftDoor_Rotation/RightDoor_Rotation floats into the AnimInstance.)
	// Signed by OpenDirection so the pair swings away from the player.
	UPROPERTY(BlueprintReadOnly, Category = "Door")
	float DoorState = 0.0f;

	// Which single leaf swings on this interaction — the one the player is in
	// front of (chosen in Interact_Implementation from the player's lateral side
	// relative to the door's center). The other leaf stays shut.
	UPROPERTY(BlueprintReadOnly, Category = "Door")
	bool bLeftLeafActive = true;
};
