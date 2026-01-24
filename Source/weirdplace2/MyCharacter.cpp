// Fill out your copyright notice in the Description page of Project Settings.

#include "MyCharacter.h"
#include "Inventory.h"
#include "InventoryRoomComponent.h"

AMyCharacter::AMyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create and attach the inventory component
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

	// Create and attach the inventory room component
	InventoryRoomComponent = CreateDefaultSubobject<UInventoryRoomComponent>(TEXT("InventoryRoomComponent"));
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
	PlayerInputComponent->BindAction("ToggleInventory", IE_Pressed, this, &AMyCharacter::OnToggleInventoryRoom);
}

void AMyCharacter::OnToggleInventoryRoom()
{
	if (InventoryRoomComponent)
	{
		InventoryRoomComponent->ToggleInventoryRoom();
	}
}

void AMyCharacter::SetCanInteract(bool value)
{
	CanInteract = value;
}

void AMyCharacter::AddItemToInventory_Implementation(EInventoryItem Item)
{
	UE_LOG(LogTemp, Warning, TEXT("C++ AddItemToInventory_Implementation called (InventoryComponent ptr: %p)"), InventoryComponent);
	if (InventoryComponent)
	{
		InventoryComponent->AddItem(Item);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("C++ AddItemToInventory: InventoryComponent is null!"));
	}
}
