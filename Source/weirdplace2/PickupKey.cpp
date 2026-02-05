#include "PickupKey.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

APickupKey::APickupKey()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create collision sphere as root
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->SetSphereRadius(50.0f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionSphere->SetGenerateOverlapEvents(true);
	RootComponent = CollisionSphere;

	// Create mesh component
	KeyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeyMesh"));
	KeyMesh->SetupAttachment(RootComponent);
	KeyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APickupKey::BeginPlay()
{
	Super::BeginPlay();
}

void APickupKey::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	// Check if the overlapping actor is the player character
	AMyCharacter* Character = Cast<AMyCharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	// Add key to inventory
	UInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (Inventory)
	{
		Inventory->AddItem(KeyName);
	}

	// Play pickup sound
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	// Destroy the key actor
	Destroy();
}
