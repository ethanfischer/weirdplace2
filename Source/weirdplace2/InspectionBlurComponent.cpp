#include "InspectionBlurComponent.h"

#include "FirstPersonCharacter.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"

UInspectionBlurComponent::UInspectionBlurComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UInspectionBlurComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AFirstPersonCharacter* Character = Cast<AFirstPersonCharacter>(GetOwner());
	UCameraComponent* Camera = Character ? Character->GetFirstPersonCamera() : nullptr;
	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("InspectionBlurComponent: owner has no first-person camera; disabling"));
		SetComponentTickEnabled(false);
		return;
	}

	// VR owns the eye's focus — never apply a screen-space DoF there.
	if (!bVRChecked)
	{
		bIsVR = GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();
		bVRChecked = true;
	}
	if (bIsVR)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float Target = Character->GetActivityState() == EPlayerActivityState::Interacting ? 1.f : 0.f;
	const float NewWeight = FMath::FInterpConstantTo(Weight, Target, DeltaTime, RampSpeed);
	if (NewWeight == Weight)
	{
		return;
	}
	Weight = NewWeight;
	ApplyToCamera(Camera);
}

void UInspectionBlurComponent::ApplyToCamera(UCameraComponent* Camera) const
{
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	const bool bActive = Weight > KINDA_SMALL_NUMBER;
	PP.bOverride_DepthOfFieldFocalDistance = bActive;
	PP.bOverride_DepthOfFieldFstop = bActive;
	if (bActive)
	{
		PP.DepthOfFieldFocalDistance = FocalDistance;
		PP.DepthOfFieldFstop = FMath::Lerp(22.f, BlurredFstop, Weight);
	}
}
