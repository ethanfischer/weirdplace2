#include "BPFL_Utilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

void UBPFL_Utilities::SetShouldLookAtPlayer(bool bValue, UObject* Player, USkeletalMeshComponent* Mesh)
{
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("SetShouldLookAtPlayer: Mesh is null"));
		return;
	}

	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("SetShouldLookAtPlayer: No AnimInstance on mesh '%s' (owner: '%s')"),
			*Mesh->GetName(), Mesh->GetOwner() ? *Mesh->GetOwner()->GetName() : TEXT("null"));
		return;
	}

	UClass* AnimClass = AnimInstance->GetClass();
	if (!AnimClass)
	{
		UE_LOG(LogTemp, Error, TEXT("SetShouldLookAtPlayer: AnimClass is null"));
		return;
	}

	FBoolProperty* BoolProp = FindFProperty<FBoolProperty>(AnimClass, TEXT("ShouldLookAtPlayer"));
	if (BoolProp)
	{
		BoolProp->SetPropertyValue_InContainer(AnimInstance, bValue);
		UE_LOG(LogTemp, Log, TEXT("SetShouldLookAtPlayer: set to %d on '%s'"), bValue, Mesh->GetOwner() ? *Mesh->GetOwner()->GetName() : TEXT("null"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SetShouldLookAtPlayer: 'ShouldLookAtPlayer' not found on anim class '%s' (mesh owner: '%s')"),
			*AnimClass->GetName(), Mesh->GetOwner() ? *Mesh->GetOwner()->GetName() : TEXT("null"));
	}
}
