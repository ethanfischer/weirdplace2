#include "UI_Dialogue.h"
#include "Components/TextBlock.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tunable.h"

WP_TUNABLE_FLOAT(GDialoguePitchShift, "weird.Dialogue.PitchShift", 0.8f,
	"Pitch multiplier applied on top of the random pitch for dialogue voice and blip sounds.");
WP_TUNABLE_FLOAT(GDialogueLowPassFreq, "weird.Dialogue.LowPassFreq", 3000.f,
	"Low-pass filter cutoff frequency (Hz) for dialogue voice and blip sounds.");
void UUI_Dialogue::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	SetupTextBacking();
}

void UUI_Dialogue::SetupTextBacking()
{
	if (!Text || !WidgetTree)
	{
		return;
	}
	// Idempotent: NativeConstruct can run more than once for a reused widget.
	if (Cast<UBorder>(Text->GetParent()))
	{
		return;
	}

	UPanelWidget* Parent = Text->GetParent();
	UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(Text->Slot);
	if (!Parent || !TextSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("UUI_Dialogue::SetupTextBacking - Text not in a CanvasPanel slot; no backing plate added"));
		return;
	}

	// The Text block fills the lower canvas (top-anchored at this Y, centered).
	// Reuse that vertical anchor so the plate lands where the text already reads.
	const float TopOffset = TextSlot->GetOffsets().Top;

	// Bound the text to a fixed-width wrapping column so the plate crops to the
	// words. AutoWrapText MUST be off here: auto-wrap sizes to the allotted
	// width, which in an auto-size slot is driven by the text's own desired
	// width — a circular dependency that collapses the block to zero. A fixed
	// WrapTextAt gives a bounded, non-zero desired size.
	Text->SetAutoWrapText(false);
	Text->SetWrapTextAt(BackingTextWrapWidth);

	TextBacking = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());

	// The default UBorder brush has no draw type, so SetBrushColor alone renders
	// nothing; procedural RoundedBox fills don't render through the world-space
	// WidgetComponent path either. The engine "WhiteBrush" (a white-texture Box
	// brush) renders reliably there — copy it and tint it for a solid plate.
	FSlateBrush Brush = *FCoreStyle::Get().GetBrush("WhiteBrush");
	// Feathered backing: a UI material fades the plate from its centre opacity out
	// to transparent within BackingEdgeFeather (UV) of each edge. Computing the fade
	// in the material (rather than baking it into a 9-slice texture) keeps the centre
	// always fully opaque and makes the feather slider intuitive — low = crisp edge,
	// high = soft/wide fade — with no setting that can blank out the backdrop.
	UMaterialInterface* FeatherMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CreatedMaterials/M_DialogueBackingFeather.M_DialogueBackingFeather"));
	if (!FeatherMat)
	{
		UE_LOG(LogTemp, Error, TEXT("UUI_Dialogue: M_DialogueBackingFeather missing; backing plate will not render correctly"));
	}
	else
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(FeatherMat, this);
		Mid->SetVectorParameterValue(FName("PlateColor"), BackingColor);
		Mid->SetScalarParameterValue(FName("PlateOpacity"), BackingOpacity);
		Mid->SetScalarParameterValue(FName("FeatherPixels"), BackingEdgeFeather);
		Brush.SetResourceObject(Mid);
		Brush.DrawAs = ESlateBrushDrawType::Image;
	}
	TextBacking->SetBrush(Brush);
	TextBacking->SetPadding(BackingPadding);
	TextBacking->SetHorizontalAlignment(HAlign_Fill);
	TextBacking->SetVerticalAlignment(VAlign_Fill);

	// Detach Text and wrap it in the border, then add the border to the canvas
	// as a FRESH slot. Mutating Text's existing (live) slot to auto-size didn't
	// take — the slot kept its offset-based 0x0 size. A freshly-added slot
	// applies auto-size cleanly, so the plate sizes to the wrapped text.
	Parent->RemoveChild(Text);
	TextBacking->SetContent(Text);
	UCanvasPanelSlot* BorderSlot = Cast<UCanvasPanelSlot>(Parent->AddChild(TextBacking));
	if (BorderSlot)
	{
		BorderSlot->SetAnchors(FAnchors(0.5f, 0.f));
		BorderSlot->SetAlignment(FVector2D(0.5f, 0.f));
		BorderSlot->SetAutoSize(true);
		BorderSlot->SetPosition(FVector2D(0.f, TopOffset));
	}
}

bool UUI_Dialogue::HasTextBacking() const
{
	return TextBacking != nullptr && Text != nullptr && Text->GetParent() == TextBacking;
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
