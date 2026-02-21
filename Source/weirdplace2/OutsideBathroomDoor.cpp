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
	// If key was already dropped, behave as a normal locked door
	if (bDidDropKey)
	{
		Super::Interact_Implementation();
		return;
	}

	// Check if player has the key
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter);
	if (!MyCharacter)
	{
		Super::Interact_Implementation();
		return;
	}

	UInventoryComponent* Inventory = MyCharacter->GetInventoryComponent();
	if (!Inventory || !Inventory->HasItem(KeyToRemove))
	{
		// Player doesn't have the key - standard locked behavior
		Super::Interact_Implementation();
		return;
	}

	// Scripted key drop event
	Inventory->RemoveItem(KeyToRemove);
	bDidDropKey = true;

	UE_LOG(LogTemp, Log, TEXT("OutsideBathroomDoor - Key '%s' dropped!"), *KeyToRemove.ToString());

	if (KeyDropSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, KeyDropSound, GetActorLocation());
	}

	// Play locked door sound as feedback (door doesn't open)
	if (LockedDoorSound)
	{
		UGameplayStatics::PlaySound2D(this, LockedDoorSound);
	}

	// Notify Seneca to move to smoking position
	if (SenecaRef)
	{
		SenecaRef->OnKeyDropped();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OutsideBathroomDoor - SenecaRef is not set, cannot notify key drop"));
	}
}
