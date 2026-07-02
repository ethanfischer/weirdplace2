#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InspectionBlurComponent.generated.h"

class UCameraComponent;

// Ramps a cinematic depth-of-field onto the first-person camera while the
// player inspects an item (activity state Interacting): the held item stays
// in focus, the world behind it blurs, and it fully clears on exit.
// Flatscreen only — the headset owns the eye's focus in VR and screen-space
// DoF there is a nausea risk.
UCLASS(ClassGroup = (Custom))
class WEIRDPLACE2_API UInspectionBlurComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInspectionBlurComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Distance the inspected item is held at — kept in crisp focus.
	UPROPERTY(EditAnywhere, Category = "Inspection Blur")
	float FocalDistance = 45.f;

	// Aperture at full blur; lower = stronger background bokeh. f/22 at
	// weight 0 is visually no blur.
	UPROPERTY(EditAnywhere, Category = "Inspection Blur")
	float BlurredFstop = 1.0f;

	// Constant ramp speed in weight/second (0->1 in 1/RampSpeed seconds).
	UPROPERTY(EditAnywhere, Category = "Inspection Blur")
	float RampSpeed = 3.f;

private:
	void ApplyToCamera(UCameraComponent* Camera) const;

	float Weight = 0.f;
	bool bIsVR = false;
	bool bVRChecked = false;
};
