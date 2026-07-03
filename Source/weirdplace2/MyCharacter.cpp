// Fill out your copyright notice in the Description page of Project Settings.

#include "MyCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "KeypadUIComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Scalability.h"

AMyCharacter::AMyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create and attach the inventory component
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

	// Create and attach the inventory UI component
	InventoryUIComponent = CreateDefaultSubobject<UInventoryUIComponent>(TEXT("InventoryUIComponent"));

	// Create and attach the keypad UI component (code-entry on locked doors)
	KeypadUIComponent = CreateDefaultSubobject<UKeypadUIComponent>(TEXT("KeypadUIComponent"));
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

void AMyCharacter::LockMovieCollection()
{
	bMovieCollectionLocked = true;
	UE_LOG(LogTemp, Log, TEXT("AMyCharacter::LockMovieCollection - Movie collection locked"));
}

void AMyCharacter::SetCanInteract(bool value)
{
	CanInteract = value;
}

void AMyCharacter::BeginInteractionHold(bool bFreezeLook)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	PC->SetIgnoreMoveInput(true);
	if (bFreezeLook)
	{
		PC->SetIgnoreLookInput(true);
	}

	SetCanInteract(false);
	SetActivityState(EPlayerActivityState::Interacting);

	// Callers bind their exit/rotate actions on PC->InputComponent; ensure it exists.
	if (!PC->InputComponent)
	{
		PC->InputComponent = NewObject<UInputComponent>(PC);
		PC->InputComponent->RegisterComponent();
	}
}

void AMyCharacter::EndInteractionHold(bool bUnfreezeLook)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	PC->SetIgnoreMoveInput(false);
	if (bUnfreezeLook)
	{
		PC->SetIgnoreLookInput(false);
	}

	SetCanInteract(true);
	SetActivityState(EPlayerActivityState::FreeRoaming);
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

