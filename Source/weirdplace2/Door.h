#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "Door.generated.h"

class UStaticMeshComponent;
class UTimelineComponent;
class UCurveFloat;
class USoundBase;

UCLASS()
class WEIRDPLACE2_API ADoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADoor();

protected:
	virtual void BeginPlay() override;

public:
	// IInteractable implementation
	virtual void Interact_Implementation() override;

	// Check if player has the required key
	UFUNCTION(BlueprintCallable, Category = "Door")
	bool HasKey() const;

	UFUNCTION(BlueprintCallable, Category = "Door")
	void SetLocked(bool bLocked) { IsLocked = bLocked; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Door")
	bool IsOpen() const { return Opened; }

protected:
	// Timeline update function - called each tick of the door animation
	UFUNCTION()
	void UpdateDoorRotation(float Alpha);

	// Picks OpenDirection so the door swings away from the player
	void UpdateOpenDirection();

	// Unit vector perpendicular to the closed door panel — points "through" the
	// doorway. This is the actor's RIGHT axis at InitialYaw, because the door
	// mesh's forward runs along the panel/wall, not through it.
	FVector GetClosedThroughAxis() const;

	// Shared close logic — used by manual close and auto-close
	void CloseDoor();

	// Auto-close timer management
	void StartAutoCloseTracking();
	void StopAutoCloseTracking();

	UFUNCTION()
	void CheckAutoClose();

	UFUNCTION()
	void AutoCloseFire();

	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	UStaticMeshComponent* DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	UTimelineComponent* DoorTimeline;

	// --- Properties ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool IsLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool Opened = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName KeyName;

	// Curve for door open/close animation (0 to 1 over time)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	UCurveFloat* DoorCurve;

	// Maximum rotation angle in degrees (default 70)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float MaxDoorAngle = 70.0f;

	// Timeline play rate. Curve is 1s long, so 0.8 -> 1.25s full open
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenSpeed = 0.8f;

	// How far past the door plane (cm) the player must walk before auto-close arms
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float AutoCloseDistance = 80.0f;

	// Seconds after the player crosses through before the door swings shut
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float AutoCloseDelay = 0.8f;

	// +1 or -1; chosen at open-time so the door swings away from the player
	float OpenDirection = 1.0f;

	// World yaw the door was placed at; timeline delta is added on top
	float InitialYaw = 0.0f;

	// Sign of the player's projection onto ClosedForward at the moment the door opened
	float OpenSidePlayerSign = 0.0f;

	FTimerHandle AutoCloseCheckTimer;
	FTimerHandle AutoCloseFireTimer;

	// --- Sounds ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Sounds")
	USoundBase* DoorOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Sounds")
	USoundBase* LockedDoorSound;
};
