#include "Rick.h"
#include "FirstPersonCharacter.h"
#include "Seneca.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "UI_Dialogue.h"
#include "Components/WidgetComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

ARick::ARick()
{
	PrimaryActorTick.bCanEverTick = true;
}

UUI_Dialogue* ARick::GetDialogueWidget() const
{
	if (!DialogueWidgetComponent)
	{
		return nullptr;
	}
	return Cast<UUI_Dialogue>(DialogueWidgetComponent->GetUserWidgetObject());
}

void ARick::BeginPlay()
{
	Super::BeginPlay();

	// Find the dialogue widget component inside a Child Actor Component (same pattern as Seneca)
	TArray<UChildActorComponent*> ChildActorComponents;
	GetComponents<UChildActorComponent>(ChildActorComponents);
	for (UChildActorComponent* ChildActorComp : ChildActorComponents)
	{
		if (ChildActorComp->GetName().Contains(TEXT("WorldSpace_UI_Dialogue")))
		{
			if (AActor* ChildActor = ChildActorComp->GetChildActor())
			{
				DialogueWidgetComponent = ChildActor->FindComponentByClass<UWidgetComponent>();
				if (DialogueWidgetComponent)
				{
					// A semi-transparent dialogue backing plate (UUI_Dialogue) needs
					// alpha blending. Force Transparent blend so BackingOpacity is a
					// true gradient -- Masked blend clips it binary at the 0.333 mask
					// threshold, which reads as a hard step near ~0.35.
					DialogueWidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);
				}
			}
			break;
		}
	}

	DialogueScript.Load(DialogueFilePath);
	LoadDialogueFile();
	LoadOutsideDialogue();
}

void ARick::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Billboard dialogue widget toward player camera
	if (DialogueWidgetComponent)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			FVector CamLocation;
			FRotator CamRotation;
			PC->GetPlayerViewPoint(CamLocation, CamRotation);
			FVector WidgetLocation = DialogueWidgetComponent->GetComponentLocation();
			FRotator LookAtRot = (CamLocation - WidgetLocation).Rotation();
			DialogueWidgetComponent->SetWorldRotation(LookAtRot);
		}
	}
}

void ARick::LoadDialogueFile()
{
	ParsedLines.Empty();
	BladderPulseLineIndex = INDEX_NONE;

	const TArray<FDialogueLine>* Section = DialogueScript.FindSection(CarRideSection);
	if (!Section)
	{
		return;
	}

	for (const FDialogueLine& Parsed : *Section)
	{
		FSimpleDialogueLine DialogueLine;
		DialogueLine.Speaker = FText::FromString(Parsed.Speaker.IsEmpty() ? TEXT("Rick") : *Parsed.Speaker);
		DialogueLine.Text = FText::FromString(Parsed.Text);
		DialogueLine.PauseAfter = Parsed.PauseAfter;
		DialogueLine.PauseBefore = Parsed.PauseBefore;
		ParsedLines.Add(DialogueLine);

		// [Bladder] tag marks the transition point — pulse fires after this line
		if (Parsed.Tag.Equals(TEXT("Bladder"), ESearchCase::IgnoreCase))
		{
			BladderPulseLineIndex = ParsedLines.Num() - 1;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Rick - Loaded %d dialogue lines from section %s"), ParsedLines.Num(), *CarRideSection);
}

void ARick::LoadOutsideDialogue()
{
	// The plate names the speaker, so idle line bodies drop any speaker prefix.
	if (const TArray<FDialogueLine>* IdleSection = DialogueScript.FindSection(OutsideIdleSection))
	{
		for (const FDialogueLine& Parsed : *IdleSection)
		{
			OutsideIdleLines.Add(FText::FromString(Parsed.Text));
		}
	}

	MoneyGiveLineIndex = INDEX_NONE;
	if (const TArray<FDialogueLine>* MoneySection = DialogueScript.FindSection(GivesMoneySection))
	{
		for (const FDialogueLine& Parsed : *MoneySection)
		{
			FSimpleDialogueLine DL;
			DL.Speaker = FText::FromString(Parsed.Speaker.IsEmpty() ? TEXT("Rick") : *Parsed.Speaker);
			DL.Text = FText::FromString(Parsed.Text);
			DL.PauseAfter = Parsed.PauseAfter;
			DL.PauseBefore = Parsed.PauseBefore;
			GivesMoneyLines.Add(DL);

			// [Gives Cash] cue — money is handed over on this line
			if (!Parsed.Tag.IsEmpty())
			{
				MoneyGiveLineIndex = GivesMoneyLines.Num() - 1;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("Rick - Loaded %d money lines, MoneyGiveLineIndex=%d"), GivesMoneyLines.Num(), MoneyGiveLineIndex);
	}
}

void ARick::StartDialogue()
{
	if (ParsedLines.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rick::StartDialogue - No parsed lines"));
		return;
	}

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (AFirstPersonCharacter* FPCharacter = Cast<AFirstPersonCharacter>(PlayerCharacter))
	{
		FPCharacter->StartDialogue(ParsedLines, this);
	}
}

void ARick::OnDialogueEnded()
{
	UE_LOG(LogTemp, Log, TEXT("Rick::OnDialogueEnded"));

	// Safety cleanup in case dialogue ended before reaching the money line
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(PlayerCharacter))
	{
		FPChar->OnDialogueLineShown.RemoveDynamic(this, &ARick::OnMoneyDialogueLineShown);
	}

	OnRickDialogueEnded.Broadcast();
}

void ARick::OnMoneyDialogueLineShown(int32 LineIndex)
{
	if (LineIndex != MoneyGiveLineIndex)
	{
		return;
	}

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(PlayerCharacter);
	if (!FPChar)
	{
		UE_LOG(LogTemp, Error, TEXT("Rick::OnMoneyDialogueLineShown - no FPChar"));
		return;
	}

	if (!bMoneyBeatArmed)
	{
		// First broadcast: arm the beat so the next E press triggers the item display
		UE_LOG(LogTemp, Log, TEXT("Rick::OnMoneyDialogueLineShown - Arming money beat at LineIndex=%d"), LineIndex);
		bMoneyBeatArmed = true;
		FPChar->bBlockNextDialogueAdvance = true;
		return;
	}

	// Second broadcast: give money + show mesh, let next E press dismiss
	bMoneyBeatArmed = false;
	FPChar->OnDialogueLineShown.RemoveDynamic(this, &ARick::OnMoneyDialogueLineShown);

	UInventoryComponent* Inventory = FPChar ? FPChar->GetInventoryComponent() : nullptr;
	if (!Inventory || !MoneyDef)
	{
		UE_LOG(LogTemp, Error, TEXT("Rick::OnMoneyDialogueLineShown - missing Inventory or MoneyDef"));
		return;
	}

	FInventoryItemData ItemData = MoneyDef->ToInventoryItemData();
	Inventory->AddItemWithData(ItemData);
	bGaveMoney = true;
	UE_LOG(LogTemp, Log, TEXT("Rick - Gave Money to player"));

	FPChar->ShowItemNotification(ItemData, MoneyDef->NotificationRotation);
}

void ARick::StashForStorm()
{
	if (bStashedForStorm)
	{
		return;
	}
	bStashedForStorm = true;
	PreStormTransform = GetActorTransform();
	SetActorEnableCollision(false);
	SetActorLocation(GetActorLocation() - FVector(0.f, 0.f, 10000.f));
	UE_LOG(LogTemp, Log, TEXT("Rick: stashed for the storm (teleported below level, collision off)"));
}

void ARick::ReturnFromStorm()
{
	if (!bStashedForStorm)
	{
		return;
	}
	bStashedForStorm = false;
	SetActorTransform(PreStormTransform);
	SetActorEnableCollision(true);
	UE_LOG(LogTemp, Log, TEXT("Rick: returned from the storm stash"));
}

void ARick::AppearOutside()
{
	if (!OutsidePositionTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("Rick::AppearOutside - OutsidePositionTarget not assigned on level instance"));
		return;
	}
	SetActorLocation(OutsidePositionTarget->GetActorLocation());

	SetActorRotation(FRotator(0.f, 180.f, 0.f));
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetVisibility(true, true);
	}
	SetActorEnableCollision(true);

	if (CarActor)
	{
		FVector CarTarget = OutsidePositionTarget->GetActorLocation() + CarActorOffset;
		CarActor->SetActorLocation(CarTarget);
		CarActor->SetActorRotation(OutsidePositionTarget->GetActorRotation());
		UE_LOG(LogTemp, Log, TEXT("Rick - Moved car to %s (OutsideTarget=%s, Offset=%s)"),
			*CarTarget.ToString(),
			*OutsidePositionTarget->GetActorLocation().ToString(),
			*CarActorOffset.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Rick::AppearOutside - CarActor not assigned on level instance"));
	}

	UE_LOG(LogTemp, Log, TEXT("Rick - Appeared outside store"));
}

void ARick::Interact_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("Rick::Interact_Implementation called"));

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AFirstPersonCharacter* FPCharacter = Cast<AFirstPersonCharacter>(PlayerCharacter);
	if (!FPCharacter) { UE_LOG(LogTemp, Error, TEXT("Rick::Interact - No FPCharacter")); return; }

	bool bSenecaWantsMoney = SenecaRef && SenecaRef->CurrentState == ESenecaState::WaitingForMoney;

	if (!bSenecaWantsMoney || bGaveMoney)
	{
		if (OutsideIdleLines.Num() > 0)
			FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Rick")), OutsideIdleLines, this);
		return;
	}

	if (GivesMoneyLines.Num() > 0)
	{
		// Bind line-shown handler so money is given on the correct line
		FPCharacter->OnDialogueLineShown.RemoveDynamic(this, &ARick::OnMoneyDialogueLineShown);
		FPCharacter->OnDialogueLineShown.AddDynamic(this, &ARick::OnMoneyDialogueLineShown);
		FPCharacter->StartDialogue(GivesMoneyLines, this);
	}
}
