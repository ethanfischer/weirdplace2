#include "StorySubsystem.h"

#include "Engine/TriggerBox.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

void UStorySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Bind the store-entry trigger so real gameplay (player walking in) drives
	// the TV switch. E2E goes through HandleStoreEntry directly. There are two
	// trigger boxes in the level (Inside/Outside) so we disambiguate by the
	// editor label (available in PIE) or an explicit actor tag for packaged.
	for (TActorIterator<ATriggerBox> It(&InWorld); It; ++It)
	{
		AActor* Trigger = *It;
		bool bMatch = Trigger->ActorHasTag(FName("StoreEntryTrigger"));
#if WITH_EDITOR
		bMatch = bMatch || Trigger->GetActorLabel() == TEXT("TriggerBox_Inside");
#endif
		if (bMatch)
		{
			Trigger->OnActorBeginOverlap.AddDynamic(this, &UStorySubsystem::OnInsideTriggerOverlap);
			UE_LOG(LogTemp, Log, TEXT("StorySubsystem: bound store-entry overlap on '%s'"), *Trigger->GetName());
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("StorySubsystem: TriggerBox_Inside not found; store-entry overlap not bound (TriggerStoreEntry still works)"));
}

void UStorySubsystem::SetFlag(EStoryFlag Flag, bool bValue)
{
	const bool bWasSet = ActiveFlags.Contains(Flag);
	if (bValue == bWasSet)
	{
		return;
	}

	if (bValue)
	{
		ActiveFlags.Add(Flag);
	}
	else
	{
		ActiveFlags.Remove(Flag);
	}

	UE_LOG(LogTemp, Log, TEXT("StorySubsystem: flag %d -> %s"), static_cast<int32>(Flag), bValue ? TEXT("true") : TEXT("false"));
	OnStoryFlagChanged.Broadcast(Flag, bValue);
}

bool UStorySubsystem::IsFlagSet(EStoryFlag Flag) const
{
	return ActiveFlags.Contains(Flag);
}

bool UStorySubsystem::TryParseStoryFlag(FName Name, EStoryFlag& OutFlag)
{
	const UEnum* Enum = StaticEnum<EStoryFlag>();
	if (!Enum)
	{
		return false;
	}
	const int64 Value = Enum->GetValueByNameString(Name.ToString());
	if (Value == INDEX_NONE)
	{
		return false;
	}
	OutFlag = static_cast<EStoryFlag>(Value);
	return true;
}

void UStorySubsystem::HandleStoreEntry()
{
	// Item 1 fills in the TV switch here. Foundation just records the beat.
	UE_LOG(LogTemp, Log, TEXT("StorySubsystem: HandleStoreEntry (KeyBroke=%d, TornadoWarningDisplayed=%d)"),
		IsFlagSet(EStoryFlag::KeyBroke) ? 1 : 0, IsFlagSet(EStoryFlag::TornadoWarningDisplayed) ? 1 : 0);
}

void UStorySubsystem::OnInsideTriggerOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor && OtherActor == PlayerPawn)
	{
		HandleStoreEntry();
	}
}
