#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DialogueWidgetProvider.generated.h"

UINTERFACE()
class UDialogueWidgetProvider : public UInterface { GENERATED_BODY() };

class WEIRDPLACE2_API IDialogueWidgetProvider
{
	GENERATED_BODY()
public:
	virtual class UUI_Dialogue* GetDialogueWidget() const = 0;
};
