#pragma once

#include "CoreMinimal.h"
#include "Door.h"
#include "OutsideBathroomDoor.generated.h"

class ASeneca;
class USoundBase;

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

	// Reference to Seneca so we can notify her when the key is dropped
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor")
	ASeneca* SenecaRef;

	// Sound to play when the key is dropped
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|Sounds")
	USoundBase* KeyDropSound;

	// Key name to remove from inventory (should match Seneca's KeyToGive)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor")
	FName KeyToRemove = FName("Key");
};
