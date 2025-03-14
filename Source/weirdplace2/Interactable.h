#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(NotBlueprintable) // This prevents Unreal from expecting Blueprint implementation
class UInteractable : public UInterface
{
    GENERATED_BODY()
};

class WEIRDPLACE2_API IInteractable
{
    GENERATED_BODY()

public:
    /** Function can be called in Blueprint but must be implemented in C++ */
    UFUNCTION(BlueprintCallable, Category="Interaction")
    virtual void InteractWithObject(AActor* Actor, float inspectionDistance) = 0;
};
