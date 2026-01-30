// Fill out your copyright notice in the Description page of Project Settings.

#include "MyCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"

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
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UE_LOG(LogTemp, Warning, TEXT("AMyCharacter::SetupPlayerInputComponent called"));

	// Bind to "ToggleInventory" action (defined in DefaultInput.ini with Tab + Gamepad_Special_Left)
	PlayerInputComponent->BindAction("ToggleInventory", IE_Pressed, this, &AMyCharacter::OnToggleInventory);
}

void AMyCharacter::OnToggleInventory()
{
	UE_LOG(LogTemp, Warning, TEXT("AMyCharacter::OnToggleInventory called! InventoryUIComponent = %p"), InventoryUIComponent);
	if (InventoryUIComponent)
	{
		InventoryUIComponent->ToggleInventoryUI();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryUIComponent is null!"));
	}
}

void AMyCharacter::SetCanInteract(bool value)
{
	CanInteract = value;
}

void AMyCharacter::AddItemToInventory_Implementation(const FName& ItemID)
{
	UE_LOG(LogTemp, Log, TEXT("AddItemToInventory called with: %s"), *ItemID.ToString());
	if (InventoryComponent)
	{
		InventoryComponent->AddItem(ItemID);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AddItemToInventory: InventoryComponent is null!"));
	}
}
