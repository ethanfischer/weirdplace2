#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupKey.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS()
class WEIRDPLACE2_API APickupKey : public AActor
{
	GENERATED_BODY()

public:
	APickupKey();

protected:
	virtual void BeginPlay() override;

public:
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

protected:
	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Key")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Key")
	UStaticMeshComponent* KeyMesh;

	// --- Properties ---

	// The name/ID of this key (used to match with doors)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key")
	FName KeyName;

	// Sound played when key is picked up
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key")
	USoundBase* PickupSound;
};
