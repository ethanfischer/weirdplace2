#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "GameFramework/Actor.h"
#include "Seneca.generated.h"

class USkeletalMeshComponent;
class USphereComponent;
class UWidgetComponent;
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

	// Widget component hosting the dialogue UI - auto-found by name in BeginPlay
	UPROPERTY(BlueprintReadOnly, Category = "Seneca|Dialogue")
	UWidgetComponent* DialogueWidgetComponent;

protected:
	// Sphere overlap callbacks
	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// --- Components (assigned in Blueprint) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca")
	USkeletalMeshComponent* BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca")
	USphereComponent* TriggerSphere;

	// --- Properties ---

	// Dialogue asset to start when player enters sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Dialogue")
	UDlgDialogue* Dialogue;

	// Radius of the dialogue trigger sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Dialogue")
	float DialogueTriggerRadius = 200.0f;
};
