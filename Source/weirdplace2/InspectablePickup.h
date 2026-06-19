#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "InspectablePickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UDiegeticTextComponent;
class UItemDefinition;
class UMaterialInterface;
class USoundBase;
class AMyCharacter;
class APlayerController;

UCLASS()
class WEIRDPLACE2_API AInspectablePickup : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AInspectablePickup();

	virtual void BeginPlay() override;

	virtual void Interact_Implementation() override;
	virtual bool CanInteract() override;

	void SetItemDef(UItemDefinition* InDef) { ItemDef = InDef; }

	void CollectInspectedItem();
	void RotateInspectedActor(float AxisValue);
	void StopInspection();

	// True while this pickup is pulled in front of the camera (between
	// Interact and StopInspection). Mirrors AMovieBox::IsBeingInspected so
	// the E2E TestDriver can find the pickup that's currently being inspected.
	bool IsBeingInspected() const { return InspectedActor != nullptr; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	UStaticMeshComponent* PickupMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	UItemDefinition* ItemDef;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	USoundBase* PickupSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InspectionDistance = 30.0f;

private:
	UPROPERTY()
	UDiegeticTextComponent* PutBackPrompt = nullptr;

	// Self-illumination overlay applied to the pickup mesh during inspection so
	// reward/inspected items read in the dark (M_ItemDarkGlow). Emissive-only.
	UPROPERTY()
	UMaterialInterface* GlowMaterial = nullptr;

	AActor* InspectedActor = nullptr;
	FTransform OriginalActorTransform;
	FRotator CameraRotation;
	APlayerController* PlayerController = nullptr;
	AMyCharacter* MyCharacter = nullptr;
	bool bCollected = false;
};
