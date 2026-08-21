#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(BlueprintType)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class WEIRDPLACE2_API IInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact();

	// Return false to suppress interaction (e.g. locked, not yet available).
	// Default returns true.
	virtual bool CanInteract() { return true; }

	// Return false to suppress interaction for a specific hit component — for
	// actors where only part of the geometry is the interactable (e.g. the
	// payphone kiosk, not the telephone pole it shares an actor with).
	// Default accepts any component.
	virtual bool IsComponentInteractable(const UPrimitiveComponent* Component) { return true; }
};
