#include "Rick.h"
#include "FirstPersonCharacter.h"
#include "Seneca.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "UI_Dialogue.h"
#include "Components/WidgetComponent.h"
#include "DialoguePlateComponent.h"
#include "NpcPersonalSpaceComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "GazeUtils.h"
#include "StorySubsystem.h"
#include "Tunable.h"

// Rick only walks in once both his current spot and the counter spot are this
// far outside the player's view cone (dot of camera forward vs direction to the
// spot). 0.3 ~= 72 degrees off-center, past the edge of a 90-degree FOV, so the
// move is never on screen.
WP_TUNABLE_FLOAT(GRickTeleportHiddenDot, "weird.Rick.TeleportHiddenDot", 0.3f,
	"Rick teleports inside once the dot between camera forward and the direction to both his spot and the counter spot drops below this.");

namespace
{
	// Each line's text from a Rick.txt section (the plate names the speaker).
	void LoadSectionText(const FDialogueScript& Script, const FString& SectionName, TArray<FText>& Out)
	{
		const TArray<FDialogueLine>* Section = Script.FindSection(SectionName);
		if (!Section)
		{
			UE_LOG(LogTemp, Error, TEXT("Rick - section '%s' missing from the dialogue file"), *SectionName);
			return;
		}
		for (const FDialogueLine& Parsed : *Section)
		{
			Out.Add(FText::FromString(Parsed.Text));
		}
	}
}

ARick::ARick()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	DialoguePlate = CreateDefaultSubobject<UDialoguePlateComponent>(TEXT("DialoguePlate"));
	PersonalSpace = CreateDefaultSubobject<UNpcPersonalSpaceComponent>(TEXT("PersonalSpace"));
}

UUI_Dialogue* ARick::GetDialogueWidget() const
{
	UWidgetComponent* Widget = DialoguePlate->GetWidgetComponent();
	if (!Widget)
	{
		return nullptr;
	}
	return Cast<UUI_Dialogue>(Widget->GetUserWidgetObject());
}

void ARick::BeginPlay()
{
	Super::BeginPlay();

	DialogueWidgetComponent = DialoguePlate->GetWidgetComponent();

	DialogueScript.Load(DialogueFilePath);
	LoadDialogueFile();
	LoadOutsideDialogue();

	GetWorld()->GetSubsystem<UStorySubsystem>()->OnPlayerEnteredStore.AddUObject(this, &ARick::OnPlayerEnteredStore);
}

void ARick::OnPlayerEnteredStore()
{
	if (bInside || bPendingTeleportInside || bStashedForStorm)
	{
		return;
	}
	if (!SenecaRef || !InsidePositionTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("Rick::OnPlayerEnteredStore - SenecaRef or InsidePositionTarget not assigned on level instance"));
		return;
	}
	// Only once the player has heard Seneca's (Czech) intro.
	if (!SenecaRef->bIntroDialoguePlayed)
	{
		return;
	}
	bPendingTeleportInside = true;
	SetActorTickEnabled(true);
	UE_LOG(LogTemp, Log, TEXT("Rick - Player came back inside after Seneca's intro; will teleport inside once unseen"));
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

	LoadSectionText(DialogueScript, IntroTranslationSection, IntroTranslationLines);
	LoadSectionText(DialogueScript, BlankTapeTranslationSection, BlankTapeTranslationLines);
	LoadSectionText(DialogueScript, KeyTranslationSection, KeyTranslationLines);
	LoadSectionText(DialogueScript, InsideIdleSection, InsideIdleLines);
}

void ARick::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Ticks only while a teleport is pending (OnPlayerEnteredStore).
	UWorld* World = GetWorld();
	if (UGazeUtils::IsPointInPlayerView(GetActorLocation(), World, GRickTeleportHiddenDot)
		|| UGazeUtils::IsPointInPlayerView(InsidePositionTarget->GetActorLocation(), World, GRickTeleportHiddenDot))
	{
		return;
	}
	TeleportInside();
}

void ARick::TeleportInside()
{
	bPendingTeleportInside = false;
	SetActorTickEnabled(false);

	if (!InsidePositionTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("Rick::TeleportInside - InsidePositionTarget not assigned on level instance"));
		return;
	}
	SetActorLocation(InsidePositionTarget->GetActorLocation());
	SetActorRotation(FRotator(0.f, InsidePositionTarget->GetActorRotation().Yaw, 0.f));
	bInside = true;
	UE_LOG(LogTemp, Log, TEXT("Rick - Teleported inside to %s"), *GetActorLocation().ToString());
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

	// Seneca may be waiting on this translation before taking the next item.
	if (bTranslationActive)
	{
		bTranslationActive = false;
		SenecaRef->OnRickTranslated();
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

bool ARick::GetCarParkingTransform(FTransform& OutTransform) const
{
	if (!OutsidePositionTarget)
	{
		return false;
	}
	OutTransform = FTransform(OutsidePositionTarget->GetActorRotation(), OutsidePositionTarget->GetActorLocation() + CarActorOffset);
	return true;
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

	// Outside at the car: always "I'll meet you inside". The money beat and the
	// translation only happen once he's in the store.
	if (!bInside)
	{
		FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Rick")), OutsideIdleLines, this);
		return;
	}

	const bool bSenecaWantsMoney = SenecaRef && SenecaRef->CurrentState == ESenecaState::WaitingForMoney;
	if (bSenecaWantsMoney && !bGaveMoney)
	{
		// Bind line-shown handler so money is given on the correct line
		FPCharacter->OnDialogueLineShown.RemoveDynamic(this, &ARick::OnMoneyDialogueLineShown);
		FPCharacter->OnDialogueLineShown.AddDynamic(this, &ARick::OnMoneyDialogueLineShown);
		FPCharacter->StartDialogue(GivesMoneyLines, this);
		return;
	}

	// Translate Seneca's open (Czech) request, as often as asked.
	if (const TArray<FText>* Translation = GetSenecaTranslation())
	{
		bTranslationActive = true;
		FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Rick")), *Translation, this);
		return;
	}

	FPCharacter->StartSimpleDialogue(FText::FromString(TEXT("Rick")), InsideIdleLines, this);
}

const TArray<FText>* ARick::GetSenecaTranslation() const
{
	if (!SenecaRef)
	{
		return nullptr;
	}
	// Rick is only inside once Seneca's intro has played.
	switch (SenecaRef->CurrentState)
	{
	case ESenecaState::WaitingForMovies:
	case ESenecaState::WaitingForMoviePurchase:
		return &IntroTranslationLines;
	case ESenecaState::WaitingForBlankTape:
		return &BlankTapeTranslationLines;
	case ESenecaState::GaveKey:
		return &KeyTranslationLines;
	default:
		return nullptr;
	}
}
