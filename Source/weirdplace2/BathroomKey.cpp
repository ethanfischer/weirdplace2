#include "BathroomKey.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "ItemDefinition.h"
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

	if (ItemDef && ItemDef->Mesh)
	{
		KeyMesh->SetStaticMesh(ItemDef->Mesh);
		KeyMesh->SetRelativeScale3D(ItemDef->Scale);
		if (ItemDef->MaterialOverrides.Num() > 0)
		{
			for (int32 i = 0; i < ItemDef->MaterialOverrides.Num(); i++)
			{
				KeyMesh->SetMaterial(i, ItemDef->MaterialOverrides[i]);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BathroomKey '%s': ItemDef missing or has no Mesh"), *GetName());
	}
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

	if (!ItemDef || ItemDef->ItemID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("BathroomKey '%s': ItemDef missing or has no ItemID. Skipping inventory add."), *GetName());
		return;
	}

	Inventory->AddItemWithData(ItemDef->ToInventoryItemData());

	// Play pickup sound
	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	// Destroy the key actor
	Destroy();
}
