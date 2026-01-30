#include "UI_DialogueOption.h"
#include "Components/TextBlock.h"
#include "DlgSystem/DlgContext.h"

void UUI_DialogueOption::Update(UDlgContext* Context)
{
	if (!Context || !OptionText)
	{
		return;
	}

	// Get the option text for this index from the dialogue context
	const TArray<FDlgEdge>& Options = Context->GetOptionsArray();
	if (Options.IsValidIndex(OptionIndex))
	{
		const FDlgEdge& Edge = Options[OptionIndex];
		OptionText->SetText(Edge.GetText());
		SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// No option at this index - hide
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UUI_DialogueOption::Highlight(UTextBlock* TextBlock)
{
	if (TextBlock)
	{
		TextBlock->SetColorAndOpacity(FSlateColor(HighlightColor));
	}
}

void UUI_DialogueOption::Unhighlight()
{
	if (OptionText)
	{
		OptionText->SetColorAndOpacity(FSlateColor(NormalColor));
	}
}

void UUI_DialogueOption::ClearText()
{
	if (OptionText)
	{
		OptionText->SetText(FText::GetEmpty());
	}
}
