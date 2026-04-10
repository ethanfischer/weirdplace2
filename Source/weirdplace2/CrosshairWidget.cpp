#include "CrosshairWidget.h"
#include "Components/Image.h"

void UCrosshairWidget::ShowNormalCrosshair()
{
	if (CrosshairImage)
	{
		CrosshairImage->SetVisibility(ESlateVisibility::Visible);
	}
	if (InteractableImage)
	{
		InteractableImage->SetVisibility(ESlateVisibility::Hidden);
	}
	if (DialogueImage)
	{
		DialogueImage->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UCrosshairWidget::ShowInteractableCrosshair()
{
	if (CrosshairImage)
	{
		CrosshairImage->SetVisibility(ESlateVisibility::Hidden);
	}
	if (InteractableImage)
	{
		InteractableImage->SetVisibility(ESlateVisibility::Visible);
	}
	if (DialogueImage)
	{
		DialogueImage->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UCrosshairWidget::ShowDialogueCrosshair()
{
	if (CrosshairImage)
	{
		CrosshairImage->SetVisibility(ESlateVisibility::Hidden);
	}
	if (InteractableImage)
	{
		InteractableImage->SetVisibility(ESlateVisibility::Hidden);
	}
	if (DialogueImage)
	{
		DialogueImage->SetVisibility(ESlateVisibility::Visible);
	}
}
