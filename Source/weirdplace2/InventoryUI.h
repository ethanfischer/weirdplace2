#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InventoryUI.generated.h"

UINTERFACE(BlueprintType)
class UInventoryUI : public UInterface
{
	GENERATED_BODY()
};

class WEIRDPLACE2_API IInventoryUI
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory UI")
	void OpenUI();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory UI")
	void CloseUI();
};
