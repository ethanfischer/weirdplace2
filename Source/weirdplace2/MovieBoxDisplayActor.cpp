#include "MovieBoxDisplayActor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AMovieBoxDisplayActor::AMovieBoxDisplayActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetRelativeScale3D(BoxScale);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AMovieBoxDisplayActor::BeginPlay()
{
	Super::BeginPlay();
}

void AMovieBoxDisplayActor::SetCoverMaterial(UMaterialInterface* InMaterial)
{
	if (Mesh && InMaterial)
	{
		Mesh->SetMaterial(0, InMaterial);
	}
}
