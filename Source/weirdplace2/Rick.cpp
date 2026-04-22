#include "Rick.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "Seneca.h"
#include "Inventory.h"
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
			}
			break;
		}
	}

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
	FString FullPath = FPaths::ProjectContentDir() / CarDialoguePath;
	TArray<FString> RawLines;
	if (!FFileHelper::LoadFileToStringArray(RawLines, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Rick - Failed to load dialogue file: %s"), *FullPath);
		return;
	}

	ParsedLines.Empty();
	BladderPulseLineIndex = INDEX_NONE;
	for (const FString& Line : RawLines)
	{
		if (Line.IsEmpty())
		{
			continue;
		}

		if (Line.TrimStartAndEnd().Equals(TEXT("[Bladder]"), ESearchCase::IgnoreCase))
		{
			// Tag marks the transition point — pulse fires after the preceding line
			BladderPulseLineIndex = ParsedLines.Num() - 1;
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
		MoneyGiveLineIndex = INDEX_NONE;
		for (const FString& Raw : MoneyRaw)
		{
			FString Line = Raw;
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty()) continue;

			// [Action] cue — ties to the preceding display line
			if (Line.StartsWith(TEXT("[")) && Line.EndsWith(TEXT("]")))
			{
				if (GivesMoneyLines.Num() > 0)
				{
					MoneyGiveLineIndex = GivesMoneyLines.Num() - 1;
				}
				continue;
			}

			int32 ColonIndex;
			if (Line.FindChar(TEXT(':'), ColonIndex))
			{
				FSimpleDialogueLine DL;
				DL.Speaker = FText::FromString(Line.Left(ColonIndex).TrimStartAndEnd());
				DL.Text    = FText::FromString(Line.Mid(ColonIndex + 1).TrimStartAndEnd());
				GivesMoneyLines.Add(DL);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("Rick - Loaded %d money lines, MoneyGiveLineIndex=%d"), GivesMoneyLines.Num(), MoneyGiveLineIndex);
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

	AMyCharacter* MyChar = Cast<AMyCharacter>(PlayerCharacter);
	UInventoryComponent* Inventory = MyChar ? MyChar->GetInventoryComponent() : nullptr;
	if (!Inventory || !MoneyMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("Rick::OnMoneyDialogueLineShown - missing Inventory or MoneyMesh"));
		return;
	}

	FInventoryItemData ItemData;
	ItemData.ItemID = FName("Money");
	ItemData.Mesh   = MoneyMesh;
	ItemData.Scale  = MoneyScale;
	for (int32 i = 0; i < MoneyMesh->GetStaticMaterials().Num(); i++)
		ItemData.Materials.Add(MoneyMesh->GetMaterial(i));

	Inventory->AddItemWithData(ItemData);
	bGaveMoney = true;
	UE_LOG(LogTemp, Log, TEXT("Rick - Gave Money to player"));

	FPChar->ShowItemNotification(ItemData);
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
