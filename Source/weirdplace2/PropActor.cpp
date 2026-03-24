#include "PropActor.h"
#include "Components/StaticMeshComponent.h"

APropActor::APropActor()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
}

void APropActor::Interact_Implementation()
{
	OnInteracted.Broadcast();
}
