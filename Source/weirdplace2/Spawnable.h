#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Spawnable.generated.h"

UINTERFACE(BlueprintType)
class USpawnable : public UInterface
{
	GENERATED_BODY()
};

class WEIRDPLACE2_API ISpawnable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Spawning")
	void Spawn(int32 Index, const TArray<FName>& Names);
};
