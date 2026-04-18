#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_DialogueOption.generated.h"

class UTextBlock;

UCLASS()
class WEIRDPLACE2_API UUI_DialogueOption : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void Highlight(UTextBlock* TextBlock);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void Unhighlight();

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void ClearText();

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* OptionText;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 OptionIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FLinearColor HighlightColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FLinearColor NormalColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
};
