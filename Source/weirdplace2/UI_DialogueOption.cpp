#include "UI_DialogueOption.h"
#include "Components/TextBlock.h"

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
