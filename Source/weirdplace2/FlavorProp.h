#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "Engine/StaticMeshActor.h"
#include "FlavorProp.generated.h"

class USoundBase;

// A static-mesh prop the player can poke for its own sake: interacting plays
// one of InteractSounds at the prop. No story effect. Derives from
// AStaticMeshActor so any existing StaticMeshActor Blueprint (BP_GasPump etc.)
// can be reparented onto it without re-authoring its mesh component.
// Subclasses (AGasPump) layer a bespoke response on top of the sound.
UCLASS()
class WEIRDPLACE2_API AFlavorProp : public AStaticMeshActor, public IInteractable
{
	GENERATED_BODY()

public:
	AFlavorProp();

	virtual void Interact_Implementation() override;
	virtual bool CanInteract() override;

	// One of these plays (random pick) on each interact. Empty = silent prop.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flavor Prop")
	TArray<USoundBase*> InteractSounds;

	// Minimum seconds between interacts, so a held key doesn't machine-gun the sound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flavor Prop", meta = (ClampMin = "0.0"))
	float InteractCooldown = 0.5f;

protected:
	// Hook for subclasses; runs after the sound on an accepted interact.
	virtual void OnFlavorInteract() {}

private:
	double LastInteractTime = -1.0;
};
