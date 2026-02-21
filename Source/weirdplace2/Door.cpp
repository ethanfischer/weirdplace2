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

	// Create root mesh component
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	RootComponent = DoorMesh;

	// Create timeline component
	DoorTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DoorTimeline"));
}

void ADoor::BeginPlay()
{
	Super::BeginPlay();

	// Setup timeline with curve
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
	if (IsLocked)
	{
		if (HasKey())
		{
			// Unlock the door
			IsLocked = false;

			// Toggle open/close
			if (Opened)
			{
				Opened = false;
				if (DoorTimeline)
				{
					DoorTimeline->Reverse();
				}
			}
			else
			{
				Opened = true;
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
		else
		{
			// Play locked sound
			if (LockedDoorSound)
			{
				UGameplayStatics::PlaySound2D(this, LockedDoorSound);
			}
		}
	}
	else
	{
		// Door is unlocked - toggle open/close
		if (Opened)
		{
			Opened = false;
			if (DoorTimeline)
			{
				DoorTimeline->Reverse();
			}
		}
		else
		{
			Opened = true;
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
}

bool ADoor::HasKey() const
{
	if (KeyName.IsNone())
	{
		return false; // No key configured — only unlockable via SetLocked()
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	AMyCharacter* Character = Cast<AMyCharacter>(PC->GetPawn());
	if (!Character)
	{
		return false;
	}

	UInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (!Inventory)
	{
		return false;
	}

	return Inventory->HasItem(KeyName);
}

void ADoor::UpdateDoorRotation(float Alpha)
{
	if (DoorMesh)
	{
		float NewYaw = FMath::Lerp(0.0f, MaxDoorAngle, Alpha);
		FRotator NewRotation(0.0f, NewYaw, 0.0f);
		DoorMesh->SetRelativeRotation(NewRotation);
	}
}
