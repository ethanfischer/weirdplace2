#include "UI_Dialogue.h"
#include "Components/TextBlock.h"
#include "Components/AudioComponent.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Kismet/GameplayStatics.h"
#include "Tunable.h"

WP_TUNABLE_FLOAT(GDialoguePitchShift, "weird.Dialogue.PitchShift", 0.55f,
	"Pitch multiplier applied on top of the random pitch for dialogue voice and blip sounds.");
WP_TUNABLE_FLOAT(GDialogueLowPassFreq, "weird.Dialogue.LowPassFreq", 300.f,
	"Low-pass filter cutoff frequency (Hz) for dialogue voice and blip sounds.");
// Dialogue text size in the widget's authored 1000x500 px space (the plate is
// 200cm wide in the world, so 1 px = 0.2cm). Sized to read beside the NPC's
// head from weird.Npc.PersonalSpaceRadius.
WP_TUNABLE_FLOAT(GDialogueTextSize, "weird.Dialogue.TextSize", 26.f,
	"Dialogue line font size (authored px). Speaker name is 1.5x this.");
// Wrap column for the line (authored px). BesideHead: ~28 characters at the
// default size, so a sentence reads as a short block beside the head instead of
// a banner across the screen. Centered (car windshield): the original width.
WP_TUNABLE_FLOAT(GDialogueWrapWidth, "weird.Dialogue.WrapWidth", 420.f,
	"Dialogue text wrap width (authored px) beside the head.");
WP_TUNABLE_FLOAT(GDialogueWrapWidthCentered, "weird.Dialogue.WrapWidthCentered", 700.f,
	"Dialogue text wrap width (authored px) for the centered (car ride) layout.");
// Black outline replaces the old dark backing plate for legibility.
WP_TUNABLE_FLOAT(GDialogueOutlineSize, "weird.Dialogue.OutlineSize", 2.f,
	"Black outline width (authored px) on dialogue text.");
// Supersampling for the world-space plate: the render target, fonts, wrap width
// and padding are all multiplied by this, and the widget component is scaled
// down by the same factor, so the plate keeps its world size but renders at
// higher resolution (kills the pixelated look up close / at 4K).
WP_TUNABLE_FLOAT(GDialogueRenderScale, "weird.Dialogue.RenderScale", 2.f,
	"Resolution multiplier for the world-space dialogue plate render target.");

float UUI_Dialogue::GetRenderScale()
{
	return GDialogueRenderScale;
}

void UUI_Dialogue::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	SetupTextStyle();
}

void UUI_Dialogue::SetupTextStyle()
{
	if (!Text || !WidgetTree)
	{
		return;
	}
	UCanvasPanel* Parent = Cast<UCanvasPanel>(Text->GetParent());
	UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(Text->Slot);
	if (!Parent || !TextSlot)
	{
		UE_LOG(LogTemp, Error, TEXT("UUI_Dialogue::SetupTextStyle - Text is not in a CanvasPanel slot"));
		return;
	}
	CenteredTopOffset = TextSlot->GetOffsets().Top;

	// Everything in px space scales with the plate's supersampling factor so
	// the world-space result is resolution-independent.
	const float RS = GDialogueRenderScale;
	FSlateFontInfo TextFont = Text->GetFont();
	TextFont.Size = GDialogueTextSize * RS;
	TextFont.OutlineSettings.OutlineSize = FMath::RoundToInt(GDialogueOutlineSize * RS);
	TextFont.OutlineSettings.OutlineColor = FLinearColor::Black;
	Text->SetFont(TextFont);
	if (SpeakerName)
	{
		FSlateFontInfo SpeakerFont = SpeakerName->GetFont();
		SpeakerFont.Size = GDialogueTextSize * 1.5f * RS;
		SpeakerFont.OutlineSettings = TextFont.OutlineSettings;
		SpeakerName->SetFont(SpeakerFont);
	}

	// AutoWrapText MUST be off: auto-wrap sizes to the allotted width, which in
	// an auto-size slot is driven by the text's own desired width — a circular
	// dependency that collapses the block to zero. A fixed WrapTextAt gives a
	// bounded, non-zero desired size.
	Text->SetAutoWrapText(false);

	// Re-add Text so it gets a FRESH slot: mutating the live slot to auto-size
	// doesn't take (it keeps its offset-based 0x0 size).
	Parent->RemoveChild(Text);
	Parent->AddChild(Text);
	ApplyLayout();
}

void UUI_Dialogue::SetPlateLayout(EDialoguePlateLayout InLayout)
{
	Layout = InLayout;
	ApplyLayout();
}

void UUI_Dialogue::ApplyLayout()
{
	UCanvasPanelSlot* TextSlot = Text ? Cast<UCanvasPanelSlot>(Text->Slot) : nullptr;
	if (!TextSlot)
	{
		return;
	}
	const float RS = GDialogueRenderScale;
	TextSlot->SetAutoSize(true);
	switch (Layout)
	{
	case EDialoguePlateLayout::BesideHead:
		TextSlot->SetAnchors(FAnchors(0.f, 0.5f));
		TextSlot->SetAlignment(FVector2D(0.f, 0.5f));
		TextSlot->SetPosition(FVector2D::ZeroVector);
		Text->SetWrapTextAt(GDialogueWrapWidth * RS);
		Text->SetJustification(ETextJustify::Left);
		break;
	case EDialoguePlateLayout::Centered:
		TextSlot->SetAnchors(FAnchors(0.5f, 0.f));
		TextSlot->SetAlignment(FVector2D(0.5f, 0.f));
		TextSlot->SetPosition(FVector2D(0.f, CenteredTopOffset * RS));
		Text->SetWrapTextAt(GDialogueWrapWidthCentered * RS);
		Text->SetJustification(ETextJustify::Center);
		break;
	}
}

bool UUI_Dialogue::HasTextOutline() const
{
	return Text != nullptr && Text->GetFont().OutlineSettings.OutlineSize > 0;
}

void UUI_Dialogue::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC, false);
	}

}

void UUI_Dialogue::OpenWithText(const FText& Speaker, const FText& DialogueLine)
{
	SetVisibility(ESlateVisibility::Visible);
	UpdateWithText(Speaker, DialogueLine);
}

void UUI_Dialogue::UpdateWithText(const FText& Speaker, const FText& DialogueLine)
{
	ClearSpeakerText();

	if (SpeakerName)
	{
		SpeakerName->SetText(Speaker);

	}

	if (VoiceSound && (!IsValid(SpawnedSound) || !SpawnedSound->IsPlaying()))
	{
		float RandomPitch = FMath::RandRange(0.75f, 1.25f) * GDialoguePitchShift;
		float RandomStartTime = FMath::RandRange(0.0f, 3.0f);
		SpawnedSound = UGameplayStatics::SpawnSound2D(GetWorld(), VoiceSound, 1.0f, RandomPitch, RandomStartTime);
		if (IsValid(SpawnedSound))
		{
			SpawnedSound->SetLowPassFilterEnabled(true);
			SpawnedSound->SetLowPassFilterFrequency(GDialogueLowPassFreq);
		}
	}

	Typewriter.OnUpdate = [this](const FString& DisplayText)
	{
		if (Text)
		{
			Text->SetText(FText::FromString(DisplayText));
		}
	};
	// Blip with randomized pitch on non-whitespace characters.
	Typewriter.OnCharacterRevealed = [this](TCHAR NewChar)
	{
		if (BlipSound && !FChar::IsWhitespace(NewChar))
		{
			float Pitch = FMath::RandRange(0.8f, 1.2f) * GDialoguePitchShift;
			// SpawnSound2D (not PlaySound2D) so we get a component to apply the LPF to.
			if (UAudioComponent* Blip = UGameplayStatics::SpawnSound2D(GetWorld(), BlipSound, 1.0f, Pitch))
			{
				Blip->SetLowPassFilterEnabled(true);
				Blip->SetLowPassFilterFrequency(GDialogueLowPassFreq);
			}
		}
	};
	Typewriter.OnFinished = [this]()
	{
		if (IsValid(SpawnedSound))
		{
			SpawnedSound->Stop();
		}
	};
	Typewriter.Start(this, DialogueLine.ToString(), /*CharInterval*/ 0.03f, /*FirstCharDelay*/ 0.04f);
}

void UUI_Dialogue::ClearSpeakerText()
{
	if (SpeakerName)
	{
		SpeakerName->SetText(FText::GetEmpty());
	}
	if (Text)
	{
		Text->SetText(FText::GetEmpty());
	}
}

void UUI_Dialogue::SetTextColor(const FSlateColor& Color)
{
	if (SpeakerName)
	{
		SpeakerName->SetColorAndOpacity(Color);
	}
	if (Text)
	{
		Text->SetColorAndOpacity(Color);
	}
}

FString UUI_Dialogue::GetDisplayedSpeaker() const
{
	return SpeakerName ? SpeakerName->GetText().ToString() : FString();
}

void UUI_Dialogue::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		SetUserFocus(PC);
	}
}
