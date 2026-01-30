// Fill out your copyright notice in the Description page of Project Settings.

#include "MyCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "CrosshairWidget.h"
#include "Interactable.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"

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

	// Create and display crosshair widget
	if (CrosshairWidgetClass)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			CrosshairWidget = CreateWidget<UCrosshairWidget>(PC, CrosshairWidgetClass);
			if (CrosshairWidget)
			{
				CrosshairWidget->AddToViewport();
				CrosshairWidget->ShowNormalCrosshair();
			}
		}
	}
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update crosshair based on what we're looking at
	if (CrosshairWidget)
	{
		bool bLookingAtInteractable = false;

		// Check if inventory is open - look for items in inventory
		if (InventoryUIComponent && InventoryUIComponent->IsInventoryOpen())
		{
			bLookingAtInteractable = InventoryUIComponent->IsLookingAtItem();
		}
		else
		{
			// Inventory closed - raycast for world interactables
			APlayerController* PC = Cast<APlayerController>(GetController());
			if (PC)
			{
				FVector CameraLocation;
				FRotator CameraRotation;
				PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

				FVector TraceEnd = CameraLocation + CameraRotation.Vector() * InteractionDistance;

				FHitResult HitResult;
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(this);

				if (GetWorld()->LineTraceSingleByChannel(HitResult, CameraLocation, TraceEnd, ECC_Visibility, QueryParams))
				{
					AActor* HitActor = HitResult.GetActor();
					if (HitActor && HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
					{
						bLookingAtInteractable = true;
					}
				}
			}
		}

		// Only update crosshair if state changed
		if (bLookingAtInteractable != bWasLookingAtInteractable)
		{
			if (bLookingAtInteractable)
			{
				CrosshairWidget->ShowInteractableCrosshair();
			}
			else
			{
				CrosshairWidget->ShowNormalCrosshair();
			}
			bWasLookingAtInteractable = bLookingAtInteractable;
		}
	}
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
