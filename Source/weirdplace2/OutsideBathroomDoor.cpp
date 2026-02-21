#include "OutsideBathroomDoor.h"
#include "Seneca.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AOutsideBathroomDoor::AOutsideBathroomDoor()
{
	// Outside bathroom door is locked by default
	IsLocked = true;
}

void AOutsideBathroomDoor::Interact_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor::Interact_Implementation CALLED. bDidDropKey=%d, IsLocked=%d"), bDidDropKey, IsLocked);

	// If key was already dropped, behave as a normal locked door
	if (bDidDropKey)
	{
		UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - Key already dropped, falling through to Super"));
		Super::Interact_Implementation();
		return;
	}

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter);
	if (!MyCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor - Could not get AMyCharacter"));
		Super::Interact_Implementation();
		return;
	}

	UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor - No InventoryComponent on character"));
		Super::Interact_Implementation();
		return;
	}

	FName ActiveItem = Inventory->GetActiveItem();
	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - ActiveItem='%s', KeyToRemove='%s', HasKey=%d"),
		*ActiveItem.ToString(), *KeyToRemove.ToString(), Inventory->HasItem(KeyToRemove));

	if (ActiveItem != KeyToRemove)
	{
		UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - Active item does not match key, playing locked sound"));
		if (LockedDoorSound)
		{
			UGameplayStatics::PlaySound2D(this, LockedDoorSound);
		}
		return;
	}

	// Scripted key drop event
	Inventory->RemoveItem(KeyToRemove);
	Inventory->ClearActiveItem();
	bDidDropKey = true;

	UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - Key '%s' dropped!"), *KeyToRemove.ToString());

	if (KeyDropSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, KeyDropSound, GetActorLocation());
	}

	if (LockedDoorSound)
	{
		UGameplayStatics::PlaySound2D(this, LockedDoorSound);
	}

	if (SenecaRef)
	{
		SenecaRef->OnKeyDropped();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OutsideBathroomDoor - SenecaRef is NOT SET, cannot notify key drop"));
	}
}
