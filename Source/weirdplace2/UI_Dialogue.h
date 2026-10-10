#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Typewriter.h"
#include "UI_Dialogue.generated.h"

class UTextBlock;
class UAudioComponent;

// How the dialogue text block is laid out inside the world-space widget.
enum class EDialoguePlateLayout : uint8
{
	// Pinned to the widget's left-middle edge, narrow wrap, left-justified:
	// the hosting UWidgetComponent uses a (0, 0.5) pivot so the plate reads
	// beside an NPC's head and grows away from it.
	BesideHead,
	// Top-center anchored at the authored offset, wide wrap, centered: the car
	// ride parks the widget on the windshield and reads it head-on.
	Centered,
};

UCLASS()
class WEIRDPLACE2_API UUI_Dialogue : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void Close();

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void OpenWithText(const FText& Speaker, const FText& DialogueLine);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void UpdateWithText(const FText& Speaker, const FText& DialogueLine);

	// Override the text color for both speaker name and dialogue text
	void SetTextColor(const FSlateColor& Color);

	// What the widget is currently showing (FullText is the complete line the
	// typewriter is revealing). Used by E2E asserts on displayed dialogue.
	FString GetDisplayedSpeaker() const;
	FString GetFullLineText() const { return Typewriter.GetFullText(); }

	// True once the dialogue Text block carries its black outline (see
	// SetupTextStyle). Used by E2E asserts.
	bool HasTextOutline() const;

	// Switch the text block's anchoring/wrap (see EDialoguePlateLayout).
	void SetPlateLayout(EDialoguePlateLayout InLayout);

	// True while the dialogue widget itself is showing (not collapsed).
	bool IsDialogueOpen() const { return GetVisibility() == ESlateVisibility::Visible; }

	// Supersampling factor (weird.Dialogue.RenderScale) the hosting
	// UWidgetComponent must apply to its draw size / inverse scale.
	static float GetRenderScale();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

	// UI Elements
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* SpeakerName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* Text;

	// Sound to play during typewriter effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Audio")
	USoundBase* VoiceSound;

	// Per-character blip sound with randomized pitch
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Audio")
	USoundBase* BlipSound;

private:
	// Fonts, outline, wrap and a fresh auto-sizing canvas slot for Text.
	void SetupTextStyle();
	void ApplyLayout();

	EDialoguePlateLayout Layout = EDialoguePlateLayout::BesideHead;
	// Authored top offset of the Text block, reused by the Centered layout.
	float CenteredTopOffset = 0.f;

	void ClearSpeakerText();

	UPROPERTY()
	UAudioComponent* SpawnedSound;

	FTypewriterReveal Typewriter;
};
