#include "DoubleDoor.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

ADoubleDoor::ADoubleDoor()
{
	// Skeletal root carries the rigged two-leaf mesh + AnimBP (assigned in BP).
	DoorSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DoorSkeletalMesh"));
	SetRootComponent(DoorSkeletalMesh);

	// The inherited static DoorMesh is unused on the double door — park it under
	// the skeletal root and hide it so ADoor's plumbing stays valid.
	if (DoorMesh)
	{
		DoorMesh->SetupAttachment(DoorSkeletalMesh);
		DoorMesh->SetVisibility(false);
	}

	// Each leaf gets its own timeline so they open/close independently.
	LeftLeafTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("LeftLeafTimeline"));
	RightLeafTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("RightLeafTimeline"));
}

void ADoubleDoor::BeginPlay()
{
	Super::BeginPlay();

	// Bind each leaf timeline to the shared DoorCurve (the inherited single
	// DoorTimeline stays set up but unused — this door drives the two leaves).
	if (DoorCurve)
	{
		FOnTimelineFloat LeftCb;
		LeftCb.BindUFunction(this, FName("UpdateLeftLeaf"));
		LeftLeafTimeline->AddInterpFloat(DoorCurve, LeftCb);
		LeftLeafTimeline->SetLooping(false);
		LeftLeafTimeline->SetPlayRate(OpenSpeed);

		FOnTimelineFloat RightCb;
		RightCb.BindUFunction(this, FName("UpdateRightLeaf"));
		RightLeafTimeline->AddInterpFloat(DoorCurve, RightCb);
		RightLeafTimeline->SetLooping(false);
		RightLeafTimeline->SetPlayRate(OpenSpeed);
	}
}

// Sets a BlueprintReadWrite float variable on a Blueprint AnimInstance by name.
// BP_Anim_DoubleDoors exposes LeftDoor_Rotation / RightDoor_Rotation, which its
// AnimGraph converts to the two leaf bone rotations. BP "real" variables compile
// to FDoubleProperty in UE5, but tolerate FFloatProperty too.
static void SetAnimFloat(UAnimInstance* Anim, FName PropName, double Value)
{
	FProperty* Prop = Anim->GetClass()->FindPropertyByName(PropName);
	if (!Prop)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleDoor] AnimInstance %s has no float var '%s'"),
			*Anim->GetClass()->GetName(), *PropName.ToString());
		return;
	}
	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
	{
		DoubleProp->SetPropertyValue_InContainer(Anim, Value);
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
	{
		FloatProp->SetPropertyValue_InContainer(Anim, static_cast<float>(Value));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleDoor] AnimInstance var '%s' is not a float/double"),
			*PropName.ToString());
	}
}

// Reads a float/double AnimInstance variable by name. Returns false if absent.
static bool GetAnimFloat(const UAnimInstance* Anim, FName PropName, float& OutValue)
{
	FProperty* Prop = Anim->GetClass()->FindPropertyByName(PropName);
	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
	{
		OutValue = static_cast<float>(DoubleProp->GetPropertyValue_InContainer(Anim));
		return true;
	}
	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
	{
		OutValue = FloatProp->GetPropertyValue_InContainer(Anim);
		return true;
	}
	return false;
}

bool ADoubleDoor::GetLeafAngles(float& OutLeft, float& OutRight) const
{
	const UAnimInstance* Anim = DoorSkeletalMesh ? DoorSkeletalMesh->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return false;
	}
	return GetAnimFloat(Anim, TEXT("LeftDoor_Rotation"), OutLeft)
		&& GetAnimFloat(Anim, TEXT("RightDoor_Rotation"), OutRight);
}

// Pushes one leaf's bone-rotation float into the AnimInstance without touching
// the other — so leaves hold independent open amounts.
void ADoubleDoor::PushLeafAngle(bool bRight, float Angle)
{
	UAnimInstance* Anim = DoorSkeletalMesh ? DoorSkeletalMesh->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleDoor %s] PushLeafAngle: no AnimInstance"), *GetName());
		return;
	}
	SetAnimFloat(Anim, bRight ? TEXT("RightDoor_Rotation") : TEXT("LeftDoor_Rotation"), Angle);
}

void ADoubleDoor::UpdateLeftLeaf(float Alpha)
{
	// Negate the swing dir: the rig's leaf bone rotates opposite the static-door
	// convention, so this swings away from the player.
	PushLeafAngle(/*bRight*/ false, Alpha * MaxDoorAngle * -LeftLeafDir);
}

void ADoubleDoor::UpdateRightLeaf(float Alpha)
{
	PushLeafAngle(/*bRight*/ true, Alpha * MaxDoorAngle * -RightLeafDir);
}

bool ADoubleDoor::IsAimingRightLeaf() const
{
	if (DoorSkeletalMesh)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			FVector ViewLoc;
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
			const FVector TraceEnd = ViewLoc + ViewRot.Vector() * 1000.0f;

			FHitResult Hit;
			if (DoorSkeletalMesh->LineTraceComponent(Hit, ViewLoc, TraceEnd,
				FCollisionQueryParams(FName(TEXT("DoubleDoorLeafPick")), /*bTraceComplex*/ false)))
			{
				// Which side of the door's center the aim landed on, along the
				// panel width axis (actor forward). >=0 is the right leaf.
				const FVector ToHit = Hit.ImpactPoint - GetActorLocation();
				return FVector::DotProduct(GetActorForwardVector(), ToHit) >= 0.0f;
			}
			UE_LOG(LogTemp, Warning,
				TEXT("[DoubleDoor %s] leaf-pick aim trace missed; defaulting to left leaf"), *GetName());
		}
	}
	return false;
}

void ADoubleDoor::Interact_Implementation()
{
	// This door ships unlocked; defer to the inherited lock/keypad path if it
	// ever gets locked.
	if (IsLocked)
	{
		Super::Interact_Implementation();
		return;
	}

	// Toggle ONLY the leaf the player is aiming at — independently of the other.
	const bool bRight = IsAimingRightLeaf();
	const bool bThisLeafOpen = bRight ? bRightLeafOpen : bLeftLeafOpen;
	if (bThisLeafOpen)
	{
		CloseLeaf(bRight);
	}
	else
	{
		OpenLeaf(bRight);
	}
}

void ADoubleDoor::OpenLeaf(bool bRight)
{
	// Pick the swing direction so this leaf opens away from the player.
	UpdateOpenDirection(); // sets inherited OpenDirection + OpenSidePlayerSign

	if (bRight)
	{
		bRightLeafOpen = true;
		RightLeafDir = OpenDirection;
		RightLeafOpenSide = OpenSidePlayerSign;
		RightLeafTimeline->PlayFromStart();
	}
	else
	{
		bLeftLeafOpen = true;
		LeftLeafDir = OpenDirection;
		LeftLeafOpenSide = OpenSidePlayerSign;
		LeftLeafTimeline->PlayFromStart();
	}

	// Keep the inherited Opened flag in sync so IsOpen() (and the interact
	// crosshair / E2E) reflect "any leaf open".
	Opened = bLeftLeafOpen || bRightLeafOpen;

	if (DoorOpenSound)
	{
		UGameplayStatics::PlaySound2D(this, DoorOpenSound);
	}
	StartLeafAutoCloseTracking();
}

void ADoubleDoor::CloseLeaf(bool bRight)
{
	if (bRight)
	{
		bRightLeafOpen = false;
		RightLeafTimeline->Reverse();
	}
	else
	{
		bLeftLeafOpen = false;
		LeftLeafTimeline->Reverse();
	}
	Opened = bLeftLeafOpen || bRightLeafOpen;
}

void ADoubleDoor::StartLeafAutoCloseTracking()
{
	UWorld* World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(LeafAutoCloseTimer))
	{
		World->GetTimerManager().SetTimer(LeafAutoCloseTimer, this, &ADoubleDoor::CheckLeafAutoClose, 0.2f, true);
	}
}

void ADoubleDoor::CheckLeafAutoClose()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return;
	}

	// Each open leaf closes once the player has crossed to the far side of the
	// doorway (relative to the side they opened it from) and walked clear.
	const FVector ToPlayer = PlayerPawn->GetActorLocation() - GetActorLocation();
	const float SignedSide = FVector::DotProduct(GetClosedThroughAxis(), ToPlayer);
	const float CurrentSign = SignedSide > 0.0f ? 1.0f : -1.0f;
	const bool bWalkedClear = FMath::Abs(SignedSide) > AutoCloseDistance;

	if (bWalkedClear && bLeftLeafOpen && CurrentSign != LeftLeafOpenSide)
	{
		CloseLeaf(/*bRight*/ false);
	}
	if (bWalkedClear && bRightLeafOpen && CurrentSign != RightLeafOpenSide)
	{
		CloseLeaf(/*bRight*/ true);
	}

	if (!bLeftLeafOpen && !bRightLeafOpen)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(LeafAutoCloseTimer);
		}
	}
}
