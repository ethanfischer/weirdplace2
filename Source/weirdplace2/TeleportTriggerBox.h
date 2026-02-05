#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "TeleportTriggerBox.generated.h"

UCLASS()
class WEIRDPLACE2_API ATeleportTriggerBox : public ATriggerBox
{
	GENERATED_BODY()

public:
	ATeleportTriggerBox();

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

protected:
	// Destroys Ultra Dynamic Sky actors if present (avoids hard dependency)
	void DestroyUltraDynamicActors();

	// --- Properties ---

	// Transform to teleport the actor to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
	FTransform TeleportTransform;

	// Whether to destroy Ultra Dynamic Sky actors on teleport
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
	bool bDestroyUltraDynamicActors = false;
};
