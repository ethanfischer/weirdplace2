#include "Hudson.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "Inventory.h"
#include "UI_Dialogue.h"
#include "Components/WidgetComponent.h"
#include "Components/ChildActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

AHudson::AHudson()
{
	PrimaryActorTick.bCanEverTick = true;
}

UUI_Dialogue* AHudson::GetDialogueWidget() const
{
	if (!DialogueWidgetComponent)
	{
		return nullptr;
	}
	return Cast<UUI_Dialogue>(DialogueWidgetComponent->GetUserWidgetObject());
}

void AHudson::BeginPlay()
{
	Super::BeginPlay();

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

	LoadDialogue(HudsonIdlePath, IdleLines);
	LoadDialogue(HudsonBegPath, BegLines);
	LoadDialogue(HudsonThankYouPath, ThankYouLines);
}

void AHudson::Tick(float DeltaTime)
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

void AHudson::LoadDialogue(const FString& RelPath, TArray<FText>& OutLines)
{
	FString FullPath = FPaths::ProjectContentDir() / RelPath;
	TArray<FString> RawLines;
	if (!FFileHelper::LoadFileToStringArray(RawLines, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Hudson - Failed to load dialogue file: %s"), *FullPath);
		return;
	}

	OutLines.Empty();
	for (const FString& Line : RawLines)
	{
		if (!Line.IsEmpty())
		{
			OutLines.Add(FText::FromString(Line));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Hudson - Loaded %d lines from %s"), OutLines.Num(), *FullPath);
}

void AHudson::Interact_Implementation()
{
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	AFirstPersonCharacter* FPChar = Cast<AFirstPersonCharacter>(PlayerCharacter);
	if (!FPChar) { UE_LOG(LogTemp, Error, TEXT("Hudson::Interact - no FPChar")); return; }

	AMyCharacter* MyChar = Cast<AMyCharacter>(PlayerCharacter);
	UInventoryComponent* Inventory = MyChar ? MyChar->GetInventoryComponent() : nullptr;
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
		if (Inventory->GetActiveItem() == FName("Money"))
		{
			Inventory->RemoveItem(FName("Money"));
			CurrentState = EHudsonState::GaveMoney;
			bLastDialogueWasBeg = false;
			FPChar->StartSimpleDialogue(FText::FromString(TEXT("Hudson")), ThankYouLines, this);
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

void AHudson::OnDialogueEnded()
{
	if (CurrentState == EHudsonState::Idle && bLastDialogueWasBeg)
	{
		CurrentState = EHudsonState::AwaitingDecision;
		bLastDialogueWasBeg = false;
		UE_LOG(LogTemp, Log, TEXT("Hudson - State -> AwaitingDecision"));
	}
}
