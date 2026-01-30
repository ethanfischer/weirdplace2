#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "Door.generated.h"

class UStaticMeshComponent;
class UTimelineComponent;
class UCurveFloat;
class USoundBase;
class AMyCharacter;

UCLASS()
class WEIRDPLACE2_API ADoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADoor();

protected:
	virtual void BeginPlay() override;

public:
	// IInteractable interface
	virtual void Interact_Implementation() override;

	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTimelineComponent* DoorTimeline;

	// --- Door Settings ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bIsLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName KeyName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenAngle = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	UCurveFloat* DoorCurve;

	// --- Sounds ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* DoorOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* LockedDoorSound;

private:
	UFUNCTION()
	void UpdateDoorRotation(float Alpha);

	bool bIsOpen = false;
	FRotator ClosedRotation;

	UPROPERTY()
	AMyCharacter* CachedCharacter;
};
