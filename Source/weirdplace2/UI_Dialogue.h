#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Typewriter.h"
#include "UI_Dialogue.generated.h"

class UTextBlock;
class UAudioComponent;
class UBorder;

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

	// True once the dialogue Text block has been wrapped in the dark backing
	// plate (see SetupTextBacking). Used by E2E asserts.
	bool HasTextBacking() const;

	// True while the dialogue widget itself is showing (not collapsed).
	bool IsDialogueOpen() const { return GetVisibility() == ESlateVisibility::Visible; }

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

	// --- Text backing plate ---
	// A dark translucent UBorder wrapped around the dialogue Text at construct
	// time so the text stays legible against bright backgrounds. Auto-sizes to
	// the text, giving a crop that hugs the words. Shared by every dialogue
	// speaker (Seneca, Rick, Hudson) because they all host this same widget.

	// Fill colour of the backing plate (RGB used; alpha comes from BackingOpacity).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Backing")
	FLinearColor BackingColor = FLinearColor(0.f, 0.f, 0.f, 1.f);

	// Plate opacity as a fine 0..1 slider (the colour-picker alpha snaps coarsely).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Backing", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BackingOpacity = 0.7f;

	// Padding between the text and the plate edges (horizontal, vertical). This is
	// the main "backdrop size" control — larger values grow the opaque plate out
	// beyond the text so there is room for the feathered edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Backing")
	FMargin BackingPadding = FMargin(60.f, 40.f);

	// Soft edge fade: the width (in pixels) of the feathered border ring. The plate
	// is fully opaque inside it and fades to transparent over this many pixels at
	// every edge — a CONSTANT width regardless of how long the line is, so a long
	// plate's ends match its dead centre. Low = crisp, high = soft. Tune live.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Backing", meta = (ClampMin = "1.0", ClampMax = "120.0", UIMin = "1.0", UIMax = "80.0"))
	float BackingEdgeFeather = 18.f;

	// Wrap width for the dialogue text; also bounds the plate's width so it
	// reads as a tidy column rather than stretching the full widget.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Backing", meta = (ClampMin = "0"))
	float BackingTextWrapWidth = 700.f;

	// The backing plate (constructed in NativeConstruct; wraps Text).
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Backing")
	UBorder* TextBacking = nullptr;

private:
	// Wraps the bound Text block in TextBacking so the plate hugs the words.
	void SetupTextBacking();

	void ClearSpeakerText();

	UPROPERTY()
	UAudioComponent* SpawnedSound;

	FTypewriterReveal Typewriter;
};
