#include "DoubleDoor.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
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

void ADoubleDoor::Interact_Implementation()
{
	// Only the leaf the player is LOOKING AT swings. Decide it on the way in
	// (while still closed) by tracing the camera aim at this door and taking which
	// side of the door's center the hit lands on, along the panel width axis
	// (actor forward). Standing at one leaf but aiming at the other opens the one
	// you're aiming at. Don't change the choice when closing — keep driving the
	// same leaf so it shuts cleanly.
	if (!Opened && DoorSkeletalMesh)
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
				const FVector ToHit = Hit.ImpactPoint - GetActorLocation();
				const float SideAlongWidth = FVector::DotProduct(GetActorForwardVector(), ToHit);
				bLeftLeafActive = SideAlongWidth >= 0.0f;
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[DoubleDoor %s] leaf-pick aim trace missed; keeping previous leaf"), *GetName());
			}
		}
	}

	Super::Interact_Implementation();
}

void ADoubleDoor::ApplyOpenAmount(float Alpha)
{
	// Signed open amount (degrees), also exposed BlueprintReadOnly for debugging.
	// OpenDirection is chosen by ADoor::UpdateOpenDirection so the pair swings
	// away from the player.
	DoorState = Alpha * MaxDoorAngle * OpenDirection;

	UAnimInstance* Anim = DoorSkeletalMesh ? DoorSkeletalMesh->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DoubleDoor %s] ApplyOpenAmount: no AnimInstance"), *GetName());
		return;
	}

	// Only the active leaf (the one the player interacted in front of) swings;
	// the other stays shut. The rig's leaf bone rotates opposite the static-door
	// convention, so negate OpenDirection to swing away from the player.
	const float Leaf = Alpha * MaxDoorAngle * -OpenDirection;
	SetAnimFloat(Anim, TEXT("LeftDoor_Rotation"), bLeftLeafActive ? 0.0f : Leaf);
	SetAnimFloat(Anim, TEXT("RightDoor_Rotation"), bLeftLeafActive ? Leaf : 0.0f);
}
