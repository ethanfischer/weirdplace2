#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_DialogueOption.h"
#include "UI_Dialogue.generated.h"

class UDlgContext;
class UTextBlock;
class UPanelWidget;
class UAudioComponent;

UCLASS()
class WEIRDPLACE2_API UUI_Dialogue : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void Open(UDlgContext* Context);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void Close();

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void Update(UDlgContext* InActiveContext);

	// Show dialogue with raw text (no DlgContext needed)
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void OpenWithText(const FText& Speaker, const FText& DialogueLine);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void UpdateWithText(const FText& Speaker, const FText& DialogueLine);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

	// UI Elements
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* SpeakerName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* Text;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UPanelWidget* Options;

	// First dialogue option for highlighting (binds to widget named "FirstDialogueOption" in Designer)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UUI_DialogueOption* FirstDialogueOption;

	// Sound to play during typewriter effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Audio")
	USoundBase* VoiceSound;

private:
	void SetNextDisplayTextCharacter();
	void ClearOptionsText();
	void ClearSpeakerText();
	void UnhighlightAllOptions();

	UPROPERTY()
	UDlgContext* ActiveContext;

	UPROPERTY()
	UAudioComponent* SpawnedSound;

	FString FullText;
	FString DisplayText;
	int32 CurrentCharIndex = 0;
	int32 CurrentOptionIndex = 0;

	FTimerHandle TypewriterTimerHandle;
};
