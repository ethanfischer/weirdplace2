#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MovieBoxDisplayActor.generated.h"

UCLASS()
class WEIRDPLACE2_API AMovieBoxDisplayActor : public AActor
{
	GENERATED_BODY()

public:
	AMovieBoxDisplayActor();

	void SetCoverMaterial(UMaterialInterface* InMaterial);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="Mesh")
	UStaticMeshComponent* Mesh;
};
