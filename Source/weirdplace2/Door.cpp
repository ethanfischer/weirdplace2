#include "Door.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

ADoor::ADoor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create door mesh
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	RootComponent = DoorMesh;

	// Create timeline component
	DoorTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DoorTimeline"));
}

void ADoor::BeginPlay()
{
	Super::BeginPlay();

	// Store closed rotation
	ClosedRotation = DoorMesh->GetRelativeRotation();

	// Cache player character
	CachedCharacter = Cast<AMyCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	// Setup timeline if curve is set
	if (DoorCurve && DoorTimeline)
	{
		FOnTimelineFloat TimelineCallback;
		TimelineCallback.BindUFunction(this, FName("UpdateDoorRotation"));
		DoorTimeline->AddInterpFloat(DoorCurve, TimelineCallback);
		DoorTimeline->SetLooping(false);
	}
}

void ADoor::Interact_Implementation()
{
	if (bIsLocked)
	{
		// Check if player has the key
		bool bHasKey = false;
		if (CachedCharacter)
		{
			if (UInventoryComponent* Inventory = CachedCharacter->GetInventoryComponent())
			{
				bHasKey = Inventory->HasItem(KeyName);
			}
		}

		if (bHasKey)
		{
			// Unlock the door
			bIsLocked = false;
			// Fall through to open/close logic below
		}
		else
		{
			// Play locked sound
			if (LockedDoorSound)
			{
				UGameplayStatics::PlaySound2D(this, LockedDoorSound);
			}
			return;
		}
	}

	// Toggle door open/close
	if (bIsOpen)
	{
		// Close the door
		bIsOpen = false;
		if (DoorTimeline)
		{
			DoorTimeline->Reverse();
		}
	}
	else
	{
		// Open the door
		bIsOpen = true;
		if (DoorOpenSound)
		{
			UGameplayStatics::PlaySound2D(this, DoorOpenSound);
		}
		if (DoorTimeline)
		{
			DoorTimeline->PlayFromStart();
		}
	}
}

void ADoor::UpdateDoorRotation(float Alpha)
{
	if (!DoorMesh)
	{
		return;
	}

	// Lerp from closed rotation to open rotation
	float NewYaw = FMath::Lerp(0.0f, OpenAngle, Alpha);
	FRotator NewRotation = ClosedRotation;
	NewRotation.Yaw += NewYaw;
	DoorMesh->SetRelativeRotation(NewRotation);
}
