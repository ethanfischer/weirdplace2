#include "Rick.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "Seneca.h"
#include "Inventory.h"
#include "UI_Dialogue.h"
#include "Components/WidgetComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

ARick::ARick()
{
	PrimaryActorTick.bCanEverTick = false;
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
			}
			break;
		}
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	LoadDialogueFile();
	LoadOutsideDialogue();
}

void ARick::LoadDialogueFile()
{
	FString FullPath = FPaths::ProjectContentDir() / CarDialoguePath;
	TArray<FString> RawLines;
	if (!FFileHelper::LoadFileToStringArray(RawLines, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Rick - Failed to load dialogue file: %s"), *FullPath);
		return;
	}

	ParsedLines.Empty();
	for (const FString& Line : RawLines)
	{
		if (Line.IsEmpty())
		{
			continue;
		}

		int32 ColonIndex;
		if (Line.FindChar(TEXT(':'), ColonIndex))
		{
			FString Speaker = Line.Left(ColonIndex).TrimStartAndEnd();
			FString Text = Line.Mid(ColonIndex + 1).TrimStartAndEnd();

			FSimpleDialogueLine DialogueLine;
			DialogueLine.Speaker = FText::FromString(Speaker);
			DialogueLine.Text = FText::FromString(Text);
			ParsedLines.Add(DialogueLine);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Rick - Loaded %d dialogue lines from %s"), ParsedLines.Num(), *FullPath);
}

void ARick::LoadOutsideDialogue()
{
	FString IdlePath = FPaths::ProjectContentDir() / RickOutsideIdlePath;
	TArray<FString> IdleRaw;
	if (FFileHelper::LoadFileToStringArray(IdleRaw, *IdlePath))
	{
		for (const FString& Line : IdleRaw)
			if (!Line.IsEmpty()) OutsideIdleLines.Add(FText::FromString(Line));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Rick - Failed to load: %s"), *IdlePath);
	}

	FString MoneyPath = FPaths::ProjectContentDir() / RickGivesMoneyPath;
	TArray<FString> MoneyRaw;
	if (FFileHelper::LoadFileToStringArray(MoneyRaw, *MoneyPath))
	{
		for (const FString& Line : MoneyRaw)
		{
			if (Line.IsEmpty()) continue;
			int32 ColonIndex;
			if (Line.FindChar(TEXT(':'), ColonIndex))
			{
				FSimpleDialogueLine DL;
				DL.Speaker = FText::FromString(Line.Left(ColonIndex).TrimStartAndEnd());
				DL.Text    = FText::FromString(Line.Mid(ColonIndex + 1).TrimStartAndEnd());
				GivesMoneyLines.Add(DL);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Rick - Failed to load: %s"), *MoneyPath);
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
		FPCharacter->StartSimpleDialogueMultiSpeaker(ParsedLines, this);
	}
}

void ARick::OnDialogueEnded()
{
	UE_LOG(LogTemp, Log, TEXT("Rick::OnDialogueEnded"));
	OnRickDialogueEnded.Broadcast();
}

void ARick::AppearOutside()
{
	if (!OutsidePositionTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("Rick::AppearOutside - OutsidePositionTarget not assigned on level instance"));
		return;
	}
	SetActorLocation(OutsidePositionTarget->GetActorLocation());
	SetActorRotation(OutsidePositionTarget->GetActorRotation());
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

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

	AMyCharacter* MyCharacter = Cast<AMyCharacter>(PlayerCharacter);
	UInventoryComponent* Inventory = MyCharacter ? MyCharacter->GetInventoryComponent() : nullptr;
	if (!Inventory) { UE_LOG(LogTemp, Error, TEXT("Rick::Interact - No inventory")); return; }
	if (!MoneyMesh) { UE_LOG(LogTemp, Error, TEXT("Rick::Interact - MoneyMesh not assigned")); return; }

	FInventoryItemData ItemData;
	ItemData.ItemID = FName("Money");
	ItemData.Mesh   = MoneyMesh;
	ItemData.Scale  = MoneyScale;
	for (int32 i = 0; i < MoneyMesh->GetStaticMaterials().Num(); i++)
		ItemData.Materials.Add(MoneyMesh->GetMaterial(i));

	Inventory->AddItemWithData(ItemData);
	bGaveMoney = true;
	UE_LOG(LogTemp, Log, TEXT("Rick - Gave Money to player"));

	if (GivesMoneyLines.Num() > 0)
		FPCharacter->StartSimpleDialogueMultiSpeaker(GivesMoneyLines, this);
}
