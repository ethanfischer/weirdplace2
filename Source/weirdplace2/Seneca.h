#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "GameFramework/Actor.h"
#include "Seneca.generated.h"

class USkeletalMeshComponent;
class USphereComponent;
class UDlgDialogue;

UCLASS()
class WEIRDPLACE2_API ASeneca : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ASeneca();

protected:
	virtual void BeginPlay() override;

public:
	// IInteractable implementation
	virtual void Interact_Implementation() override;

protected:
	// Sphere overlap callbacks
	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seneca")
	USkeletalMeshComponent* Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seneca")
	USphereComponent* DialogueTriggerSphere;

	// --- Properties ---

	// Dialogue asset to start when player enters sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Dialogue")
	UDlgDialogue* Dialogue;

	// Radius of the dialogue trigger sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Dialogue")
	float DialogueTriggerRadius = 200.0f;
};
