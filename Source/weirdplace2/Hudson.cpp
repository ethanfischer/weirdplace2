#include "Hudson.h"
#include "FirstPersonCharacter.h"
#include "Inventory.h"
#include "InventoryUIComponent.h"
#include "UI_Dialogue.h"
#include "Components/WidgetComponent.h"
#include "DialoguePlateComponent.h"
#include "NpcPersonalSpaceComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

AHudson::AHudson()
{
	PrimaryActorTick.bCanEverTick = false;
	DialoguePlate = CreateDefaultSubobject<UDialoguePlateComponent>(TEXT("DialoguePlate"));
	PersonalSpace = CreateDefaultSubobject<UNpcPersonalSpaceComponent>(TEXT("PersonalSpace"));
}

UUI_Dialogue* AHudson::GetDialogueWidget() const
{
	UWidgetComponent* Widget = DialoguePlate->GetWidgetComponent();
	if (!Widget)
	{
		return nullptr;
	}
	return Cast<UUI_Dialogue>(Widget->GetUserWidgetObject());
}

void AHudson::BeginPlay()
{
	Super::BeginPlay();

	DialogueWidgetComponent = DialoguePlate->GetWidgetComponent();

	DialogueScript.Load(DialogueFilePath);
	LoadDialogue(IdleSection, IdleLines);
	LoadDialogue(BegSection, BegLines);
	LoadDialogue(ThankYouSection, ThankYouLines);
}

void AHudson::LoadDialogue(const FString& SectionName, TArray<FText>& OutLines)
{
	OutLines.Empty();
	const TArray<FDialogueLine>* Section = DialogueScript.FindSection(SectionName);
	if (!Section)
	{
		return;
	}
	for (const FDialogueLine& Line : *Section)
	{
		OutLines.Add(FText::FromString(Line.Text));
	}
	UE_LOG(LogTemp, Log, TEXT("Hudson - Loaded %d lines from section %s"), OutLines.Num(), *SectionName);
}

void AHudson::Interact_Implementation()
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(PlayerCharacter);
	if (!FPChar) { UE_LOG(LogTemp, Error, TEXT("Hudson::Interact - no FPChar")); return; }

	UInventoryComponent* Inventory = FPChar->GetInventoryComponent();
	if (!Inventory) { UE_LOG(LogTemp, Error, TEXT("Hudson::Interact - no Inventory")); return; }

	bool bHasMoney = Inventory->HasItem(FName("Money"));

	switch (CurrentState)
	{
	case EHudsonState::Idle:
		if (bHasMoney)
		{
			bLastDialogueWasBeg = true;
			FPChar->StartSimpleDialogue(FText::FromString(TEXT("Hudson")), BegLines, this);
		}
		else
		{
			bLastDialogueWasBeg = false;
			FPChar->StartSimpleDialogue(FText::FromString(TEXT("Hudson")), IdleLines, this);
		}
		break;

	case EHudsonState::AwaitingDecision:
		if (Inventory->HasItem(FName("Money")))
		{
			// Pop the inventory; the player picks Money and presses E to hand it over.
			if (UInventoryUIComponent* InvUI = FPChar->GetInventoryUIComponent())
			{
				InvUI->OpenForGive(FInventoryGiveDelegate::CreateUObject(this, &AHudson::OnMoneyOffered));
			}
		}
		else
		{
			bLastDialogueWasBeg = true;
			FPChar->StartSimpleDialogue(FText::FromString(TEXT("Hudson")), BegLines, this);
		}
		break;

	case EHudsonState::GaveMoney:
		FPChar->StartSimpleDialogue(FText::FromString(TEXT("Hudson")), ThankYouLines, this);
		break;
	}
}

bool AHudson::OnMoneyOffered(FName ItemID)
{
	if (ItemID != FName("Money"))
	{
		return false; // wrong item — keep the inventory open
	}

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(PlayerCharacter);
	UInventoryComponent* Inventory = FPChar ? FPChar->GetInventoryComponent() : nullptr;
	if (!Inventory || !FPChar)
	{
		return false;
	}

	// Consume + thank the player; ConfirmGiveSelection closes the UI on accept.
	Inventory->RemoveItem(FName("Money"));
	CurrentState = EHudsonState::GaveMoney;
	bLastDialogueWasBeg = false;
	FPChar->StartSimpleDialogue(FText::FromString(TEXT("Hudson")), ThankYouLines, this);
	return true;
}

void AHudson::OnDialogueEnded()
{
	if (CurrentState == EHudsonState::Idle && bLastDialogueWasBeg)
	{
		CurrentState = EHudsonState::AwaitingDecision;
		bLastDialogueWasBeg = false;
		UE_LOG(LogTemp, Log, TEXT("Hudson - State -> AwaitingDecision"));
	}
}
