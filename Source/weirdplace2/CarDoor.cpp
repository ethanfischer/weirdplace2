#include "CarDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"

ACarDoor::ACarDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Hinge);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Hinge);
	Mesh->SetMobility(EComponentMobility::Movable);
	// Query-only (and only while armed) so the interact trace finds it; it
	// never pushes the player around.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);

	AimPoint = CreateDefaultSubobject<USceneComponent>(TEXT("AimPoint"));
	AimPoint->SetupAttachment(Hinge);
}

void ACarDoor::SetMesh(UStaticMesh* InMesh)
{
	Mesh->SetStaticMesh(InMesh);
	// Door mesh origin is the hinge at ground level; aim at the panel's middle
	const FBoxSphereBounds Bounds = InMesh->GetBounds();
	AimPoint->SetRelativeLocation(Bounds.Origin);
}

void ACarDoor::SetArmed(bool bInArmed)
{
	bArmed = bInArmed;
	Mesh->SetCollisionEnabled(bInArmed ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void ACarDoor::SetOpenYaw(float Yaw)
{
	SetActorRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
}

void ACarDoor::Interact_Implementation()
{
	if (!bArmed)
	{
		return;
	}
	SetArmed(false);
	OnInteract.Broadcast();
}

ACarExitSpot::ACarExitSpot()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetBoxExtent(FVector(60.0f, 60.0f, 40.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldDynamic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	SetRootComponent(Box);
}

void ACarExitSpot::Interact_Implementation()
{
	if (!bArmed)
	{
		return;
	}
	bArmed = false;
	OnInteract.Broadcast();
}
