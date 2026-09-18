#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI_Dialogue.h"
#include "DialoguePlateComponent.generated.h"

class UWidgetComponent;

// Owns the diegetic dialogue plate hosted by an NPC's "WorldSpace_UI_Dialogue"
// child actor: finds the UWidgetComponent at BeginPlay and billboards it toward
// the camera every tick.
UCLASS(ClassGroup = (Dialogue), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UDialoguePlateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDialoguePlateComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UWidgetComponent* GetWidgetComponent() const { return WidgetComponent; }

	// BesideHead (default): this component places the plate beside the head
	// every tick. Centered: whoever moved the widget actor owns its position
	// (the car ride parks it on the windshield); only the billboard runs.
	void SetLayout(EDialoguePlateLayout InLayout);

private:
	UPROPERTY()
	TObjectPtr<UWidgetComponent> WidgetComponent;

	EDialoguePlateLayout Layout = EDialoguePlateLayout::BesideHead;
};
