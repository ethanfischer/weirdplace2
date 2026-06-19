#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "StorySubsystem.generated.h"

class ACRTTV;

// Central narrative-state store for the tornado/telephone beat chain. Lives on
// the world (not the game instance) so flags reset per-PIE — clean E2E isolation
// in a single-level game. Items 1/2/4/5 all gate on flags held here.
UENUM(BlueprintType)
enum class EStoryFlag : uint8
{
	// Bathroom key snapped in the lock (set additively by AOutsideBathroomDoor).
	KeyBroke,
	// The store TVs have switched to the tornado-warning screen (set once).
	TornadoWarningDisplayed,
	// Player has actually gazed at a warning TV long enough to register it.
	SeenTornadoWarning
};

// Broadcast whenever a flag's value changes. APayPhone subscribes to drive its
// reveal; other consumers may bind too.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStoryFlagChanged, EStoryFlag /*Flag*/, bool /*bValue*/);

UCLASS()
class WEIRDPLACE2_API UStorySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// Set (or clear) a flag. Broadcasts OnStoryFlagChanged only on an actual change.
	void SetFlag(EStoryFlag Flag, bool bValue = true);

	// True if the flag is currently set.
	bool IsFlagSet(EStoryFlag Flag) const;

	// Fires after SetFlag changes a flag's value.
	FOnStoryFlagChanged OnStoryFlagChanged;

	// Resolve the test-facing FName ("KeyBroke" etc.) to the enum. False on miss.
	static bool TryParseStoryFlag(FName Name, EStoryFlag& OutFlag);

	// Bring the world to the given beat: sets every flag up to and including
	// Target and runs each beat's side effect (the store-entry TV flip), so the
	// scene state matches having reached that point. Relies on EStoryFlag being
	// declared in story order. Used by the `SkipTo` dev console command.
	void SkipToBeat(EStoryFlag Target);

	// The store-entry beat: player crossed TriggerBox_Inside. If the key has
	// broken and the warning hasn't shown yet, switches both store TVs to the
	// tornado-warning screen. Also invoked directly by the TestDriver so E2E
	// doesn't have to physically walk the player through the trigger.
	void HandleStoreEntry();

private:
	UFUNCTION()
	void OnInsideTriggerOverlap(AActor* OverlappedActor, AActor* OtherActor);

	// Polls whether the player is gazing at a warning TV; sets SeenTornadoWarning
	// after the dwell. Runs on a repeating timer only while the warning is up.
	void TickGazeWatch();

	// Membership == flag is true.
	TSet<EStoryFlag> ActiveFlags;

	// TVs switched to the warning screen, watched for the player's gaze.
	UPROPERTY()
	TArray<ACRTTV*> WarningTVs;

	FTimerHandle GazeWatchTimer;
	float GazeDwellSeconds = 0.f;
};
