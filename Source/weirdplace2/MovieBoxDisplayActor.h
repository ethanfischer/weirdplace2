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

	// Name of the cover (for display when looked at)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Display")
	FString CoverName;

	void SetCoverName(const FString& Name) { CoverName = Name; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="Mesh")
	UStaticMeshComponent* Mesh;

	// Scale to approximate a VHS box aspect
	UPROPERTY(EditAnywhere, Category="Display")
	FVector BoxScale = FVector(0.1f, 0.75f, 1.0f);
};
