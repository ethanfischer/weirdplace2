#pragma once

#include "CoreMinimal.h"
#include "Door.h"
#include "OutsideBathroomDoor.generated.h"

UCLASS()
class WEIRDPLACE2_API AOutsideBathroomDoor : public ADoor
{
	GENERATED_BODY()

public:
	AOutsideBathroomDoor();

	// Override interact for bathroom-specific behavior
	virtual void Interact_Implementation() override;

protected:
	// Tracks whether the key has been dropped
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OutsideBathroomDoor")
	bool bDidDropKey = false;
};
