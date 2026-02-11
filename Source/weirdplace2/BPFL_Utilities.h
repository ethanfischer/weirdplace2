#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BPFL_Utilities.generated.h"

class USkeletalMeshComponent;

UCLASS()
class WEIRDPLACE2_API UBPFL_Utilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sets the "ShouldLookAtPlayer" variable on the animation blueprint of the given skeletal mesh.
	 * Used for NPCs to look at the player when they are nearby.
	 *
	 * @param bValue - Whether the NPC should look at the player
	 * @param Player - The player object (used to get player reference if needed)
	 * @param Mesh - The skeletal mesh component with the animation blueprint
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities")
	static void SetShouldLookAtPlayer(bool bValue, UObject* Player, USkeletalMeshComponent* Mesh);
};
