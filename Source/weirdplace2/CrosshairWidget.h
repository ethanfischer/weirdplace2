// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CrosshairWidget.generated.h"

UCLASS()
class WEIRDPLACE2_API UCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Called when looking at an interactable object - implement in Blueprint to show large crosshair
	UFUNCTION(BlueprintImplementableEvent, Category = "Crosshair")
	void ShowInteractableCrosshair();

	// Called when not looking at anything interactable - implement in Blueprint to show normal crosshair
	UFUNCTION(BlueprintImplementableEvent, Category = "Crosshair")
	void ShowNormalCrosshair();
};
