#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "GameFramework/Actor.h"
#include "PropActor.generated.h"

class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE(FOnPropInteracted);

UCLASS()
class WEIRDPLACE2_API APropActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APropActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* MeshComponent;

	virtual void Interact_Implementation() override;

	// Fires when the player interacts with this prop
	FOnPropInteracted OnInteracted;
};
