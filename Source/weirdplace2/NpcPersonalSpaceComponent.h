#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NpcPersonalSpaceComponent.generated.h"

class UCapsuleComponent;

// Keeps the player from walking into an NPC. Spawns a capsule at BeginPlay
// that blocks only the Pawn channel, so interaction traces, camera, and
// visibility pass through it. Created at runtime (not as a default subobject)
// because these NPCs get their root component from Blueprint, and a native
// scene subobject would otherwise be adopted as the root.
UCLASS(ClassGroup = (NPC), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UNpcPersonalSpaceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	UCapsuleComponent* GetCapsule() const { return Capsule; }

private:
	UPROPERTY()
	TObjectPtr<UCapsuleComponent> Capsule;
};
