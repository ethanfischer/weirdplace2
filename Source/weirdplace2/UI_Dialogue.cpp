#include "UI_Dialogue.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/AudioComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DlgSystem/DlgContext.h"

void UUI_Dialogue::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UUI_Dialogue::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC, false);
	}

	ActiveContext = nullptr;
}

void UUI_Dialogue::Update(UDlgContext* InActiveContext)
{
	if (!IsValid(InActiveContext))
	{
		Close();
		return;
	}

	ClearSpeakerText();
	ClearOptionsText();

	if (SpeakerName)
	{
		FText ParticipantName = InActiveContext->GetActiveNodeParticipantDisplayName();
		SpeakerName->SetText(ParticipantName);

	}

	const FText& NodeText = InActiveContext->GetActiveNodeText();
	FullText = NodeText.ToString();
	DisplayText.Empty();
	CurrentCharIndex = 0;

	// Store context for timer use
	ActiveContext = InActiveContext;

	// Start typewriter effect after short delay (weak-bound to prevent crash if widget destroyed)
	FTimerDelegate TimerDelegate = FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		SetNextDisplayTextCharacter();

		// Handle voice sound
		if (VoiceSound)
		{
			if (!IsValid(SpawnedSound) || !SpawnedSound->IsPlaying())
			{
				float RandomPitch = FMath::RandRange(0.75f, 1.25f);
				float RandomStartTime = FMath::RandRange(0.0f, 3.0f);
				SpawnedSound = UGameplayStatics::SpawnSound2D(GetWorld(), VoiceSound, 1.0f, RandomPitch, RandomStartTime);
			}
		}

		// Update dialogue options
		if (Options && IsValid(ActiveContext))
		{
			TArray<UWidget*> AllChildren = Options->GetAllChildren();
			for (UWidget* Child : AllChildren)
			{
				if (UUI_DialogueOption* Option = Cast<UUI_DialogueOption>(Child))
				{
					Option->Update(ActiveContext);
				}
			}
			CurrentOptionIndex = 0;
		}
	});
	GetWorld()->GetTimerManager().SetTimer(TypewriterTimerHandle, TimerDelegate, 0.04f, false);
}

void UUI_Dialogue::Open(UDlgContext* Context)
{
	UnhighlightAllOptions();
	CurrentOptionIndex = 0;
	ActiveContext = Context;
	SetVisibility(ESlateVisibility::Visible);
	Update(Context);

	// Highlight first option - requires UUI_DialogueOption implementation
}

void UUI_Dialogue::OpenWithText(const FText& Speaker, const FText& DialogueLine)
{
	UnhighlightAllOptions();
	CurrentOptionIndex = 0;
	ActiveContext = nullptr;
	SetVisibility(ESlateVisibility::Visible);
	UpdateWithText(Speaker, DialogueLine);
}

void UUI_Dialogue::UpdateWithText(const FText& Speaker, const FText& DialogueLine)
{
	ClearSpeakerText();
	ClearOptionsText();

	if (SpeakerName)
	{
		SpeakerName->SetText(Speaker);

	}

	FullText = DialogueLine.ToString();
	DisplayText.Empty();
	CurrentCharIndex = 0;

	// Start typewriter effect after short delay
	FTimerDelegate TimerDelegate = FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		SetNextDisplayTextCharacter();

		if (VoiceSound)
		{
			if (!IsValid(SpawnedSound) || !SpawnedSound->IsPlaying())
			{
				float RandomPitch = FMath::RandRange(0.75f, 1.25f);
				float RandomStartTime = FMath::RandRange(0.0f, 3.0f);
				SpawnedSound = UGameplayStatics::SpawnSound2D(GetWorld(), VoiceSound, 1.0f, RandomPitch, RandomStartTime);
			}
		}
	});
	GetWorld()->GetTimerManager().SetTimer(TypewriterTimerHandle, TimerDelegate, 0.04f, false);
}

void UUI_Dialogue::SetNextDisplayTextCharacter()
{
	if (DisplayText.Equals(FullText, ESearchCase::CaseSensitive))
	{
		// Finished typing
		if (IsValid(SpawnedSound))
		{
			SpawnedSound->Stop();
		}
		return;
	}

	// Add next character
	if (CurrentCharIndex < FullText.Len())
	{
		DisplayText.AppendChar(FullText[CurrentCharIndex]);
		CurrentCharIndex++;

		if (Text)
		{
			Text->SetText(FText::FromString(DisplayText));

		}

		// Continue typewriter effect
		GetWorld()->GetTimerManager().SetTimer(TypewriterTimerHandle, [this]()
		{
			SetNextDisplayTextCharacter();
		}, 0.03f, false);
	}
}

void UUI_Dialogue::ClearOptionsText()
{
	if (Options)
	{
		TArray<UWidget*> AllChildren = Options->GetAllChildren();
		for (UWidget* Child : AllChildren)
		{
			if (UUI_DialogueOption* Option = Cast<UUI_DialogueOption>(Child))
			{
				Option->ClearText();
			}
		}
	}
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

void UUI_Dialogue::UnhighlightAllOptions()
{
	if (Options)
	{
		TArray<UWidget*> AllChildren = Options->GetAllChildren();
		for (UWidget* Child : AllChildren)
		{
			if (UUI_DialogueOption* Option = Cast<UUI_DialogueOption>(Child))
			{
				Option->Unhighlight();
			}
		}
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

void UUI_Dialogue::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		SetUserFocus(PC);
	}
}
