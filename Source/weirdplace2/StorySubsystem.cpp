#include "StorySubsystem.h"

#include "CRTTV.h"
#include "GazeUtils.h"
#include "Engine/TriggerBox.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace StorySubsystemConst
{
	// Gaze watch: poll cadence + how long the player must look at a warning TV.
	static constexpr float GazeWatchInterval = 0.1f;
	static constexpr float GazeRequiredSeconds = 2.0f;
	// TVs are small and close; keep the box-expand tight and the range modest.
	static constexpr float GazeBoxExpand = 10.f;
	static constexpr float GazeMaxDistance = 4000.f;
}

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
	// Only the first store entry after the key breaks flips the TVs.
	if (!IsFlagSet(EStoryFlag::KeyBroke) || IsFlagSet(EStoryFlag::TornadoWarningDisplayed))
	{
		UE_LOG(LogTemp, Log, TEXT("StorySubsystem: HandleStoreEntry no-op (KeyBroke=%d, TornadoWarningDisplayed=%d)"),
			IsFlagSet(EStoryFlag::KeyBroke) ? 1 : 0, IsFlagSet(EStoryFlag::TornadoWarningDisplayed) ? 1 : 0);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	WarningTVs.Reset();
	for (TActorIterator<ACRTTV> It(World); It; ++It)
	{
		ACRTTV* TV = *It;
		TV->ShowTornadoWarning();
		WarningTVs.Add(TV);
	}
	UE_LOG(LogTemp, Log, TEXT("StorySubsystem: store entry switched %d TV(s) to tornado warning"), WarningTVs.Num());

	SetFlag(EStoryFlag::TornadoWarningDisplayed);

	// Watch for the player to actually look at a warning TV → SeenTornadoWarning.
	GazeDwellSeconds = 0.f;
	World->GetTimerManager().SetTimer(GazeWatchTimer, this, &UStorySubsystem::TickGazeWatch,
		StorySubsystemConst::GazeWatchInterval, /*bLoop*/ true);
}

void UStorySubsystem::TickGazeWatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (IsFlagSet(EStoryFlag::SeenTornadoWarning))
	{
		World->GetTimerManager().ClearTimer(GazeWatchTimer);
		return;
	}

	bool bGazing = false;
	for (ACRTTV* TV : WarningTVs)
	{
		if (TV && UGazeUtils::IsActorInPlayerGaze(TV, World,
			StorySubsystemConst::GazeBoxExpand, StorySubsystemConst::GazeMaxDistance, /*bRequireLineOfSight*/ true))
		{
			bGazing = true;
			break;
		}
	}

	if (bGazing)
	{
		GazeDwellSeconds += StorySubsystemConst::GazeWatchInterval;
		if (GazeDwellSeconds >= StorySubsystemConst::GazeRequiredSeconds)
		{
			SetFlag(EStoryFlag::SeenTornadoWarning);
			World->GetTimerManager().ClearTimer(GazeWatchTimer);
		}
	}
	else
	{
		GazeDwellSeconds = 0.f;
	}
}

void UStorySubsystem::OnInsideTriggerOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor && OtherActor == PlayerPawn)
	{
		HandleStoreEntry();
	}
}
