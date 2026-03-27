#pragma once
#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "LookAtPlayerComponent.generated.h"

class USkeletalMeshComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEIRDPLACE2_API ULookAtPlayerComponent : public USphereComponent
{
	GENERATED_BODY()
public:
	ULookAtPlayerComponent();
protected:
	virtual void BeginPlay() override;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt")
	FName BodyMeshComponentName = FName("Body");
private:
	UPROPERTY()
	USkeletalMeshComponent* BodyMesh;

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
