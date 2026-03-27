#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LookAtPlayerComponent.generated.h"

class USkeletalMeshComponent;
class USphereComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEIRDPLACE2_API ULookAtPlayerComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	ULookAtPlayerComponent();
protected:
	virtual void BeginPlay() override;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt")
	FName BodyMeshComponentName = FName("Body");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt")
	float TriggerRadius = 200.0f;
private:
	UPROPERTY()
	USkeletalMeshComponent* BodyMesh;
private:
	UPROPERTY()
	USphereComponent* TriggerSphere;

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
