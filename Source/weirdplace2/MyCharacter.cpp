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

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Bind to "ToggleInventory" action (defined in DefaultInput.ini with Tab + Gamepad_Special_Left)
	PlayerInputComponent->BindAction("ToggleInventory", IE_Pressed, this, &AMyCharacter::OnToggleInventory);
}

void AMyCharacter::OnToggleInventory()
{
	if (!bInventoryUnlocked)
	{
		UE_LOG(LogTemp, Log, TEXT("OnToggleInventory - inventory not yet unlocked (talk to Seneca first)"));
		return;
	}

	if (ActivityState != EPlayerActivityState::FreeRoaming)
	{
		return;
	}

	if (InventoryUIComponent)
	{
		InventoryUIComponent->ToggleInventoryUI();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryUIComponent is null!"));
	}
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
	ActivityState = NewState;
}

bool AMyCharacter::IsInAnyDialogue() const
{
	return ActivityState == EPlayerActivityState::InSimpleDialogue
		|| ActivityState == EPlayerActivityState::InMultiSpeakerDialogue
		|| ActivityState == EPlayerActivityState::InDlgDialogue;
}

void AMyCharacter::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	if (!bForce && (IsInAnyDialogue() || ActivityState == EPlayerActivityState::WaitingForItemInteractionInDialogue))
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
