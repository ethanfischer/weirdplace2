#include "BPFL_Utilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

void UBPFL_Utilities::SetShouldLookAtPlayer(bool bValue, UObject* Player, USkeletalMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}

	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	// Set the boolean property on the animation blueprint
	// The property name "ShouldLookAtPlayer" should match what's defined in the anim BP
	UClass* AnimClass = AnimInstance->GetClass();
	if (AnimClass)
	{
		FBoolProperty* BoolProp = FindFProperty<FBoolProperty>(AnimClass, TEXT("ShouldLookAtPlayer"));
		if (BoolProp)
		{
			BoolProp->SetPropertyValue_InContainer(AnimInstance, bValue);
		}
	}
}
