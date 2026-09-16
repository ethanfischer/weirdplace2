#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DialoguePlateComponent.generated.h"

class UWidgetComponent;
class USkeletalMeshComponent;

// Owns the diegetic dialogue plate hosted by an NPC's "WorldSpace_UI_Dialogue"
// child actor: finds the UWidgetComponent at BeginPlay, billboards it toward the
// camera every tick, and keeps it inside the viewport's safe region while the
// NPC is in view (otherwise it rests at its authored anchor).
UCLASS(ClassGroup = (Dialogue), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UDialoguePlateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDialoguePlateComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UWidgetComponent* GetWidgetComponent() const { return WidgetComponent; }

	// Authored plate position (the child actor's widget offset) in world space.
	// Follows the child actor if something relocates it (e.g. the car ride).
	FVector GetAnchorWorldLocation() const;

private:
	UPROPERTY()
	TObjectPtr<UWidgetComponent> WidgetComponent;

	// NPC body used for the view-cone test.
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> BodyMesh;

	// Widget offset/scale relative to its attach parent, cached at BeginPlay.
	FVector AnchorRelativeLocation = FVector::ZeroVector;
	FVector BaseRelativeScale = FVector::OneVector;
	FVector BaseWorldScale = FVector::OneVector;
};
