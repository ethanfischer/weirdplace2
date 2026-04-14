#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CrosshairWidget.generated.h"

class UImage;

UCLASS()
class WEIRDPLACE2_API UCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void ShowNormalCrosshair();

	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void ShowInteractableCrosshair();

	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void ShowDialogueCrosshair();

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* CrosshairImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* InteractableImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* DialogueImage;
};
