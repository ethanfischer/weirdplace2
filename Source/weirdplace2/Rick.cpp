#include "Rick.h"
#include "FirstPersonCharacter.h"
#include "Components/WidgetComponent.h"
#include "Components/ChildActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

ARick::ARick()
{
	PrimaryActorTick.bCanEverTick = false;
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

		// Format: "Speaker: Dialogue text" - split on first ": "
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
