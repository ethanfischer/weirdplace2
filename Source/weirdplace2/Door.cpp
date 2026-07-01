#include "Door.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "KeypadUIComponent.h"
#include "StorySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

ADoor::ADoor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root mesh component
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	RootComponent = DoorMesh;

	// Create timeline component
	DoorTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DoorTimeline"));

	// Obvious / cheeky codes that are rejected even though they contain an 8 or
	// would otherwise pass. Editable per-instance in the Details panel.
	BlockedKeypadCodes = {
		TEXT("1111"), TEXT("2222"), TEXT("3333"), TEXT("4444"), TEXT("5555"),
		TEXT("6666"), TEXT("7777"), TEXT("8888"), TEXT("9999"),
		TEXT("1234"), TEXT("6969"),
	};
}

void ADoor::BeginPlay()
{
	Super::BeginPlay();

	InitialYaw = GetActorRotation().Yaw;
	UE_LOG(LogTemp, Display, TEXT("[Door %s] BeginPlay InitialYaw=%.1f Location=%s"),
		*GetName(), InitialYaw, *GetActorLocation().ToString());

	// Setup timeline with curve
	if (DoorCurve && DoorTimeline)
	{
		FOnTimelineFloat TimelineCallback;
		TimelineCallback.BindUFunction(this, FName("UpdateDoorRotation"));
		DoorTimeline->AddInterpFloat(DoorCurve, TimelineCallback);
		DoorTimeline->SetLooping(false);
		DoorTimeline->SetPlayRate(OpenSpeed);
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
				CloseDoor();
			}
			else
			{
				Opened = true;
				UpdateOpenDirection();
				if (DoorOpenSound)
				{
					UGameplayStatics::PlaySound2D(this, DoorOpenSound);
				}
				if (DoorTimeline)
				{
					DoorTimeline->PlayFromStart();
				}
				StartAutoCloseTracking();
			}
		}
		else if (bUsesKeypadLock)
		{
			// Pop the world-space keypad; the door opens on the correct code.
			if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
			{
				if (UKeypadUIComponent* Keypad = MyCharacter->GetKeypadUIComponent())
				{
					Keypad->OpenForCode(KeypadCodeLength,
						FKeypadSubmitDelegate::CreateUObject(this, &ADoor::OnKeypadCodeSubmitted));
					return;
				}
			}
			// No keypad component available — fall back to the locked sound.
			if (LockedDoorSound)
			{
				UGameplayStatics::PlaySound2D(this, LockedDoorSound);
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
			CloseDoor();
		}
		else
		{
			Opened = true;
			UpdateOpenDirection();
			if (DoorOpenSound)
			{
				UGameplayStatics::PlaySound2D(this, DoorOpenSound);
			}
			if (DoorTimeline)
			{
				DoorTimeline->PlayFromStart();
			}
			StartAutoCloseTracking();
		}
	}
}

bool ADoor::IsKeypadCodeAccepted(const FString& Code) const
{
	if (!RequiredKeypadDigit.IsEmpty() && !Code.Contains(RequiredKeypadDigit))
	{
		return false; // missing the required digit (e.g. an 8)
	}
	if (BlockedKeypadCodes.Contains(Code))
	{
		return false; // too obvious / blocked
	}
	return true;
}

bool ADoor::OnKeypadCodeSubmitted(const FString& EnteredCode)
{
	// The code is mostly an illusion — almost any full-length entry is accepted,
	// but only after the player has picked up the payphone (where the "code" is
	// spoken), and only if it passes the digit/blocklist rules.
	UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr;
	if (!Story || !Story->IsFlagSet(EStoryFlag::UsedPayPhone) || !IsKeypadCodeAccepted(EnteredCode))
	{
		return false; // keypad buzzes (DenySound) + clears, stays locked
	}

	// Accept: unlock + open (same as the keyed unlock path in Interact).
	IsLocked = false;
	Opened = true;
	UpdateOpenDirection();
	if (DoorOpenSound)
	{
		UGameplayStatics::PlaySound2D(this, DoorOpenSound);
	}
	if (DoorTimeline)
	{
		DoorTimeline->PlayFromStart();
	}
	StartAutoCloseTracking();
	return true; // keypad closes on accept
}

void ADoor::CloseDoor()
{
	StopAutoCloseTracking();
	Opened = false;
	if (DoorTimeline)
	{
		DoorTimeline->Reverse();
	}
}

void ADoor::UpdateOpenDirection()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return;
	}
	const FVector ToPlayer = PlayerPawn->GetActorLocation() - GetActorLocation();
	const FVector ThroughAxis = GetClosedThroughAxis();
	const float SignedSide = FVector::DotProduct(ThroughAxis, ToPlayer);
	OpenSidePlayerSign = SignedSide > 0.0f ? 1.0f : -1.0f;
	OpenDirection = -OpenSidePlayerSign;
	UE_LOG(LogTemp, Display,
		TEXT("[Door %s] UpdateOpenDirection: InitialYaw=%.1f ThroughAxis=(%.2f,%.2f,%.2f) ToPlayer=(%.1f,%.1f,%.1f) SignedSide=%.1f -> Sign=%.0f OpenDir=%.0f"),
		*GetName(), InitialYaw, ThroughAxis.X, ThroughAxis.Y, ThroughAxis.Z,
		ToPlayer.X, ToPlayer.Y, ToPlayer.Z, SignedSide, OpenSidePlayerSign, OpenDirection);
}

FVector ADoor::GetClosedThroughAxis() const
{
	// Actor's RIGHT vector at the closed yaw — perpendicular to the door panel,
	// pointing through the doorway. (The mesh's forward runs along the panel.)
	return FRotator(0.0f, InitialYaw, 0.0f).RotateVector(FVector::RightVector);
}

void ADoor::StartAutoCloseTracking()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->GetTimerManager().SetTimer(AutoCloseCheckTimer, this, &ADoor::CheckAutoClose, 0.2f, true);
	UE_LOG(LogTemp, Display, TEXT("[Door %s] StartAutoCloseTracking armed (OpenSidePlayerSign=%.0f)"), *GetName(), OpenSidePlayerSign);
}

void ADoor::StopAutoCloseTracking()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(AutoCloseCheckTimer);
	World->GetTimerManager().ClearTimer(AutoCloseFireTimer);
}

void ADoor::CheckAutoClose()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return;
	}
	const FVector ToPlayer = PlayerPawn->GetActorLocation() - GetActorLocation();
	const float SignedSide = FVector::DotProduct(GetClosedThroughAxis(), ToPlayer);
	const float CurrentSign = SignedSide > 0.0f ? 1.0f : -1.0f;
	UE_LOG(LogTemp, Display,
		TEXT("[Door %s] CheckAutoClose: SignedSide=%.1f CurrentSign=%.0f OpenSign=%.0f (need flip + |dist|>%.0f)"),
		*GetName(), SignedSide, CurrentSign, OpenSidePlayerSign, AutoCloseDistance);

	if (CurrentSign != OpenSidePlayerSign && FMath::Abs(SignedSide) > AutoCloseDistance)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}
		World->GetTimerManager().ClearTimer(AutoCloseCheckTimer);
		if (AutoCloseDelay <= 0.0f)
		{
			AutoCloseFire();
		}
		else
		{
			World->GetTimerManager().SetTimer(AutoCloseFireTimer, this, &ADoor::AutoCloseFire, AutoCloseDelay, false);
		}
	}
}

void ADoor::AutoCloseFire()
{
	UE_LOG(LogTemp, Display, TEXT("[Door %s] AutoCloseFire"), *GetName());
	CloseDoor();
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
	ApplyOpenAmount(Alpha);
}

void ADoor::ApplyOpenAmount(float Alpha)
{
	if (DoorMesh)
	{
		const float DeltaYaw = FMath::Lerp(0.0f, MaxDoorAngle * OpenDirection, Alpha);
		DoorMesh->SetRelativeRotation(FRotator(0.0f, InitialYaw + DeltaYaw, 0.0f));
	}
}
