#pragma once

#include "CoreMinimal.h"
#include "Door.h"
#include "DoubleDoor.generated.h"

class USkeletalMeshComponent;
class UTimelineComponent;

// A glass double door whose two leaves open and close INDEPENDENTLY. Each leaf
// has its own timeline and open state, so opening one never disturbs the other.
// Interacting toggles whichever leaf the player is aiming at. Inherits ADoor's
// IInteractable plumbing, curve/sounds and lock/keypad (this door ships
// unlocked); the open/close + auto-close is per-leaf here, driving the two
// LeftDoor_Rotation/RightDoor_Rotation floats that BP_Anim_DoubleDoors poses.
UCLASS()
class WEIRDPLACE2_API ADoubleDoor : public ADoor
{
	GENERATED_BODY()

public:
	ADoubleDoor();

	// Toggles the single leaf the player is aiming at, independently of the other.
	virtual void Interact_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsLeftLeafOpen() const { return bLeftLeafOpen; }

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsRightLeafOpen() const { return bRightLeafOpen; }

	// Reads back the leaf angles currently pushed into the AnimInstance
	// (LeftDoor_Rotation/RightDoor_Rotation). Returns false if there's no
	// AnimInstance or the float vars are missing. Used by the E2E guard.
	bool GetLeafAngles(float& OutLeft, float& OutRight) const;

protected:
	virtual void BeginPlay() override;

	// One timeline callback per leaf so the leaves animate on independent clocks.
	UFUNCTION()
	void UpdateLeftLeaf(float Alpha);

	UFUNCTION()
	void UpdateRightLeaf(float Alpha);

	// Repeating check: auto-close any open leaf once the player walks through.
	UFUNCTION()
	void CheckLeafAutoClose();

	// True if the player's camera is aimed at the right leaf (else the left).
	bool IsAimingRightLeaf() const;

	void OpenLeaf(bool bRight);
	void CloseLeaf(bool bRight);
	void PushLeafAngle(bool bRight, float Angle);
	void StartLeafAutoCloseTracking();

	// Skeletal root for the rigged double-door mesh (assigned in BP, like BP_Door
	// assigns its static mesh). Replaces the inherited static DoorMesh as
	// RootComponent; DoorMesh is parked under it, hidden and unused.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	USkeletalMeshComponent* DoorSkeletalMesh;

	// One timeline per leaf — the source of independent open/close.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	UTimelineComponent* LeftLeafTimeline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	UTimelineComponent* RightLeafTimeline;

	UPROPERTY(BlueprintReadOnly, Category = "Door")
	bool bLeftLeafOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Door")
	bool bRightLeafOpen = false;

	// Swing sign per leaf, chosen at open-time so each swings away from the player.
	float LeftLeafDir = 1.0f;
	float RightLeafDir = 1.0f;

	// Which through-axis side the player was on when each leaf opened (for auto-close).
	float LeftLeafOpenSide = 0.0f;
	float RightLeafOpenSide = 0.0f;

	FTimerHandle LeafAutoCloseTimer;
};
