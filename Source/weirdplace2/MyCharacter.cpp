// Fill out your copyright notice in the Description page of Project Settings.

#include "MyCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "Scalability.h"

AMyCharacter::AMyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create and attach the inventory component
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

	// Create and attach the inventory UI component
	InventoryUIComponent = CreateDefaultSubobject<UInventoryUIComponent>(TEXT("InventoryUIComponent"));
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// DeviceProfile cvars are applied before Lumen has fully warmed up, so
	// the first frame uses stale lighting state. Re-applying scalability after
	// the world begins play forces Lumen to pick up the configured quality.
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Scalability::SetQualityLevels(Levels, /*bForce=*/true);
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
