#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "ShadowManTrigger.generated.h"

class AShadowMan;

UCLASS()
class WEIRDPLACE2_API AShadowManTrigger : public ATriggerBox
{
	GENERATED_BODY()

public:
	UPROPERTY(EditInstanceOnly, Category = "ShadowMan")
	AShadowMan* ShadowMan = nullptr;

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;
};
