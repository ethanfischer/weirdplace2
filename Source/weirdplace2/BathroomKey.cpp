#include "BathroomKey.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

ABathroomKey::ABathroomKey()
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

void ABathroomKey::BeginPlay()
{
	Super::BeginPlay();
}

void ABathroomKey::NotifyActorBeginOverlap(AActor* OtherActor)
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
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("BathroomKey '%s': Character missing InventoryComponent. Cannot add key."), *GetName());
		return;
	}

	if (KeyName == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("BathroomKey '%s' has invalid KeyName (NAME_None). Skipping inventory add."), *GetName());
		return;
	}

	// Add item with visual data captured from the mesh
	FInventoryItemData ItemData = UInventoryComponent::CreateItemDataFromMeshComponent(KeyName, KeyMesh);
	Inventory->AddItemWithData(ItemData);

	// Play pickup sound
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	// Destroy the key actor
	Destroy();
}
