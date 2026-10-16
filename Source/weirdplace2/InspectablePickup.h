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
class AFirstPersonCharacter;
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
	UItemDefinition* GetItemDef() const { return ItemDef; }
	FName GetItemID() const;

	// A pickup hidden in the level from the start (the spawner's stash behind a
	// movie box), as opposed to one dropped mid-story (broken key, gaze reward).
	// The E2E driver's "wait for a pickup to appear" skips stash pickups.
	void SetIsStash(bool bInStash) { bIsStash = bInStash; }
	bool IsStash() const { return bIsStash; }

	void CollectInspectedItem();
	void RotateInspectedActor(float AxisValue);
	void StopInspection();

	// True while this pickup is pulled in front of the camera (between
	// Interact and StopInspection). Mirrors AMovieBox::IsBeingInspected so
	// the E2E TestDriver can find the pickup that's currently being inspected.
	bool IsBeingInspected() const { return InspectedActor != nullptr; }

	// Test seam: true if the pickup mesh currently carries the self-illumination
	// glow overlay (always-on while the pickup exists, so it reads on the ground
	// in the dark, not just while inspected).
	bool IsGlowActive() const;

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
	AFirstPersonCharacter* MyCharacter = nullptr;
	bool bCollected = false;
	bool bIsStash = false;
};
