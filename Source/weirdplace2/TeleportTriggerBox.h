#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "Engine/TargetPoint.h"
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

	// Required target point actor to teleport to (set on the level instance)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Teleport")
	ATargetPoint* TeleportTarget;

	// Whether to destroy Ultra Dynamic Sky actors on teleport
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
	bool bDestroyUltraDynamicActors = false;
};
