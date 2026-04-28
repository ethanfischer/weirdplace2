// Fill out your copyright notice in the Description page of Project Settings.

#include "MyCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "HeldItemComponent.h"

AMyCharacter::AMyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create and attach the inventory component
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

	// Create and attach the inventory UI component
	InventoryUIComponent = CreateDefaultSubobject<UInventoryUIComponent>(TEXT("InventoryUIComponent"));

	// Create and attach the held item component
	HeldItemComponent = CreateDefaultSubobject<UHeldItemComponent>(TEXT("HeldItemComponent"));
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMyCharacter::LockMovieCollection()
{
	bMovieCollectionLocked = true;
	UE_LOG(LogTemp, Log, TEXT("AMyCharacter::LockMovieCollection - Movie collection locked"));
}

void AMyCharacter::UnlockInventory()
{
	if (bInventoryUnlocked)
	{
		return;
	}
	bInventoryUnlocked = true;
	UE_LOG(LogTemp, Log, TEXT("Inventory unlocked for player"));
}

void AMyCharacter::SetCanInteract(bool value)
{
	CanInteract = value;
}

void AMyCharacter::SetActivityState(EPlayerActivityState NewState)
{
	if (IsInAnyDialogue() && NewState == EPlayerActivityState::FreeRoaming)
	{
		LastDialogueEndTime = GetWorld()->GetTimeSeconds();
	}
	ActivityState = NewState;
}

bool AMyCharacter::IsDialogueCooldownActive() const
{
	return GetWorld()->GetTimeSeconds() - LastDialogueEndTime < 1.0;
}

bool AMyCharacter::IsInAnyDialogue() const
{
	return ActivityState == EPlayerActivityState::InSimpleDialogue
		|| ActivityState == EPlayerActivityState::InDialogue;
}

void AMyCharacter::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	if (!bForce && IsInAnyDialogue())
	{
		return;
	}
	Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
}

void AMyCharacter::AddItemToInventory_Implementation(const FName& ItemID)
{
	if (InventoryComponent)
	{
		InventoryComponent->AddItem(ItemID);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AddItemToInventory: InventoryComponent is null!"));
	}
}

void AMyCharacter::AddItemToInventoryWithMesh(const FName& ItemID, UStaticMeshComponent* MeshComponent)
{
	if (InventoryComponent)
	{
		FInventoryItemData ItemData = UInventoryComponent::CreateItemDataFromMeshComponent(ItemID, MeshComponent);
		InventoryComponent->AddItemWithData(ItemData);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AddItemToInventoryWithMesh: InventoryComponent is null!"));
	}
}
