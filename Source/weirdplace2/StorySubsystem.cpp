#include "StorySubsystem.h"

#include "CRTTV.h"
#include "Rick.h"
#include "FirstPersonCharacter.h"
#include "GazeUtils.h"
#include "StormFogComponent.h"
#include "Tunable.h"
#include "Components/AudioComponent.h"
#include "Components/LightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/TriggerBox.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/AmbientSound.h"
#include "TimerManager.h"

WP_TUNABLE_FLOAT(GStormDimMultiplier, "weird.Storm.DimMultiplier", 0.f,
	"Each StormDimLight-tagged light's intensity is multiplied by this when the tornado warning shows (0 = fully off).");
WP_TUNABLE_FLOAT(GRelightDelaySeconds, "weird.Relight.DelaySeconds", 0.f,
	"Seconds after the payphone hangup before the station lights flicker back on (0 = immediately).");
WP_TUNABLE_FLOAT(GRelightFlickerDuration, "weird.Relight.FlickerDuration", 2.5f,
	"Seconds of light flicker before the station lights settle fully on.");
WP_TUNABLE_FLOAT(GRelightRelaxedFogDistance, "weird.Relight.RelaxedFogDistance", 4000.f,
	"Fog distance (cm) the pea soup relaxes to on relight, so the glow reads from afar.");
WP_TUNABLE_FLOAT(GRelightFogRelaxSeconds, "weird.Relight.FogRelaxSeconds", 4.f,
	"Seconds for the fog to relax open on relight.");

namespace StorySubsystemConst
{
	// Actor tags marking the storm-beat targets in the level (a subsystem has no
	// Details panel, so tags replace the old controller's wired arrays).
	static const FName StormDimLightTag("StormDimLight");
	static const FName StormHideActorTag("StormHideActor");
	static const FName StormSilenceAmbientTag("StormSilenceAmbient");

	// Odd number of flicker toggles so the sequence starts on and ends on.
	static constexpr int32 FlickerSteps = 7;

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

	OnStoryFlagChanged.AddUObject(this, &UStorySubsystem::OnStoryFlagSet);

	// NOTE (first-play fog bug, 2026-07-02): do NOT "fix" the invisible fog
	// wall by rebuilding the fog's render state here — a mid-play
	// MarkRenderStateDirty() on the ExponentialHeightFogComponent produces
	// dead fog for the rest of the play (verified: it regressed play 2, which
	// previously always healed). Whatever recreates the fog render state
	// early in a session's first play is the actual culprit; still unsolved.

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

namespace
{
	// Single source of truth for beat naming: the friendly display name shown to
	// the dev plus the space-separated lowercase aliases SkipTo accepts for each
	// beat. The display name also matches case-insensitively.
	struct FBeatAlias
	{
		EStoryFlag Flag;
		const TCHAR* Display;
		const TCHAR* Aliases;
	};

	static const FBeatAlias GBeatAliases[] = {
		{ EStoryFlag::KeyBroke,                TEXT("KeyBroke"),       TEXT("key") },
		{ EStoryFlag::TornadoWarningDisplayed, TEXT("Tornado"), TEXT("tornadowarning tornadowarningdisplayed tv tvs") },
		{ EStoryFlag::SeenTornadoWarning,      TEXT("Telephone"),      TEXT("telephone phone payphone seentornadowarning") },
		{ EStoryFlag::UsedPayPhone,            TEXT("PhoneUsed"),      TEXT("phoneused usedpayphone calledphone offhook") },
		{ EStoryFlag::HungUpPhone,             TEXT("HungUp"),         TEXT("hungupphone hangup hungup") },
		{ EStoryFlag::StationRelit,            TEXT("Relight"),        TEXT("relight stationrelit relit generator") },
	};
}

bool UStorySubsystem::ResolveBeat(const FString& Name, EStoryFlag& OutFlag)
{
	const FString Lower = Name.ToLower();
	for (const FBeatAlias& Entry : GBeatAliases)
	{
		if (Lower == FString(Entry.Display).ToLower())
		{
			OutFlag = Entry.Flag;
			return true;
		}
		TArray<FString> Aliases;
		FString(Entry.Aliases).ParseIntoArray(Aliases, TEXT(" "), /*InCullEmpty*/ true);
		if (Aliases.Contains(Lower))
		{
			OutFlag = Entry.Flag;
			return true;
		}
	}

	// Fall back to exact enum names (e.g. a beat not in the alias table).
	return TryParseStoryFlag(FName(*Name), OutFlag);
}

FString UStorySubsystem::GetBeatDisplayName(EStoryFlag Flag)
{
	for (const FBeatAlias& Entry : GBeatAliases)
	{
		if (Entry.Flag == Flag)
		{
			return Entry.Display;
		}
	}
	const UEnum* Enum = StaticEnum<EStoryFlag>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Flag)) : TEXT("?");
}

TArray<FString> UStorySubsystem::GetBeatDisplayNames()
{
	TArray<FString> Names;
	const UEnum* Enum = StaticEnum<EStoryFlag>();
	const int32 Num = Enum ? Enum->NumEnums() - 1 : 0; // last entry is the hidden _MAX
	for (int32 i = 0; i < Num; ++i)
	{
		Names.Add(GetBeatDisplayName(static_cast<EStoryFlag>(Enum->GetValueByIndex(i))));
	}
	return Names;
}

void UStorySubsystem::SkipToBeat(EStoryFlag Target)
{
	// Beats are sequential (KeyBroke -> TornadoWarningDisplayed -> SeenTornadoWarning
	// -> UsedPayPhone -> HungUpPhone -> StationRelit), so "skip to Target" means
	// apply every beat up to and including it.
	const int32 T = static_cast<int32>(Target);

	if (T >= static_cast<int32>(EStoryFlag::KeyBroke))
	{
		SetFlag(EStoryFlag::KeyBroke, true);
	}
	if (T >= static_cast<int32>(EStoryFlag::TornadoWarningDisplayed))
	{
		// Flips both store TVs + sets TornadoWarningDisplayed (self-guards if already done).
		HandleStoreEntry();
	}
	if (T >= static_cast<int32>(EStoryFlag::SeenTornadoWarning))
	{
		SetFlag(EStoryFlag::SeenTornadoWarning, true);
	}
	if (T >= static_cast<int32>(EStoryFlag::UsedPayPhone))
	{
		SetFlag(EStoryFlag::UsedPayPhone, true);
	}
	if (T >= static_cast<int32>(EStoryFlag::HungUpPhone))
	{
		SetFlag(EStoryFlag::HungUpPhone, true);
	}
	if (T >= static_cast<int32>(EStoryFlag::StationRelit))
	{
		SetFlag(EStoryFlag::StationRelit, true);
	}
	UE_LOG(LogTemp, Log, TEXT("StorySubsystem: SkipToBeat -> %d"), T);
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

void UStorySubsystem::OnStoryFlagSet(EStoryFlag Flag, bool bValue)
{
	if (!bValue)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (Flag == EStoryFlag::TornadoWarningDisplayed)
	{
		ApplyStorm();
		return;
	}

	if (Flag == EStoryFlag::HungUpPhone)
	{
		if (!bRelit)
		{
			World->GetTimerManager().SetTimer(RelightDelayTimer, this, &UStorySubsystem::Relight,
				FMath::Max(0.01f, GRelightDelaySeconds), /*bLoop*/ false);
			UE_LOG(LogTemp, Log, TEXT("StorySubsystem: relight armed — fires in %.0fs"), GRelightDelaySeconds);
		}
		return;
	}

	if (Flag == EStoryFlag::StationRelit)
	{
		// SkipToBeat sets the flag directly — run the beat now instead of waiting
		// out the armed countdown. No-op when Relight itself set the flag.
		World->GetTimerManager().ClearTimer(RelightDelayTimer);
		Relight();
		return;
	}

	if (Flag != EStoryFlag::UsedPayPhone)
	{
		return;
	}

	// Silence every TV directly rather than the cached WarningTVs list — that list
	// is only populated by HandleStoreEntry, so depending on it here would leave
	// the siren blaring if UsedPayPhone is ever set without store-entry first.
	// StopWarningAudio is idempotent and a safe no-op on a TV that never warned.
	int32 Silenced = 0;
	for (TActorIterator<ACRTTV> It(World); It; ++It)
	{
		It->StopWarningAudio();
		++Silenced;
	}
	UE_LOG(LogTemp, Log, TEXT("StorySubsystem: payphone used, silenced %d warning siren(s)"), Silenced);
}

void UStorySubsystem::ApplyStorm()
{
	if (bStormApplied)
	{
		return;
	}
	bStormApplied = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// The storm closes in: dim/hide/silence every tagged actor. Recording each
	// light's pre-dim intensity is what makes the relight restore possible — the
	// level dims to 0, so the old divide-out-the-multiplier restore can't work.
	DimmedLights.Reset();
	DimmedOriginalIntensities.Reset();
	HiddenActors.Reset();
	int32 SilencedBeds = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->ActorHasTag(StorySubsystemConst::StormDimLightTag))
		{
			TArray<ULightComponent*> LightComps;
			Actor->GetComponents<ULightComponent>(LightComps);
			for (ULightComponent* Light : LightComps)
			{
				DimmedLights.Add(Light);
				DimmedOriginalIntensities.Add(Light->Intensity);
				Light->SetIntensity(Light->Intensity * GStormDimMultiplier);
			}
		}
		if (Actor->ActorHasTag(StorySubsystemConst::StormHideActorTag))
		{
			// Per project convention, drive visibility on the root component —
			// SetActorHiddenInGame is unreliable (the component's bVisible wins).
			if (USceneComponent* Root = Actor->GetRootComponent())
			{
				Root->SetVisibility(false, /*bPropagateToChildren*/ true);
				HiddenActors.Add(Actor);
			}
		}
		if (Actor->ActorHasTag(StorySubsystemConst::StormSilenceAmbientTag))
		{
			AAmbientSound* Ambient = Cast<AAmbientSound>(Actor);
			UAudioComponent* AudioComp = Ambient ? Ambient->GetAudioComponent() : nullptr;
			if (AudioComp)
			{
				AudioComp->Stop();
				++SilencedBeds;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("StorySubsystem: StormSilenceAmbient-tagged '%s' is not an AAmbientSound with audio"), *Actor->GetName());
			}
		}
	}

	// Rick leaves with the storm — he only comes back much later in the game.
	// MetaHuman: teleport-stash, never a visibility toggle (grooms break on re-show).
	for (TActorIterator<ARick> RickIt(World); RickIt; ++RickIt)
	{
		RickIt->StashForStorm();
	}

	UE_LOG(LogTemp, Log, TEXT("StorySubsystem: storm applied — dimmed %d light comp(s) x%.2f, hid %d actor(s), silenced %d ambient bed(s)"),
		DimmedLights.Num(), GStormDimMultiplier, HiddenActors.Num(), SilencedBeds);
}

void UStorySubsystem::Relight()
{
	if (bRelit)
	{
		return;
	}
	bRelit = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	SetFlag(EStoryFlag::StationRelit);

	if (!bStormApplied)
	{
		// A hangup without the storm beat (possible in tests and skips) makes the
		// relight a visual no-op — the lights were never dimmed.
		UE_LOG(LogTemp, Warning, TEXT("StorySubsystem: relight fired but the storm never applied; nothing to restore"));
		return;
	}

	// Flicker: an odd toggle count over FlickerDuration so the lights (and the
	// re-shown glow meshes) pulse on/off and end fully on.
	FlickerStep = 0;
	TickFlicker();
	World->GetTimerManager().SetTimer(FlickerTimer, this, &UStorySubsystem::TickFlicker,
		FMath::Max(0.05f, GRelightFlickerDuration / StorySubsystemConst::FlickerSteps), /*bLoop*/ true);

	// Open the fog with the flicker so the glow actually reads at distance —
	// at the settled ~2.5m visibility a relit station would stay invisible.
	if (AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
	{
		if (UStormFogComponent* Fog = Player->FindComponentByClass<UStormFogComponent>())
		{
			Fog->RelaxFog(GRelightRelaxedFogDistance, GRelightFogRelaxSeconds);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("StorySubsystem: relight — %d light comp(s) flickering up, %d reveal actor(s)"),
		DimmedLights.Num(), HiddenActors.Num());
}

void UStorySubsystem::TickFlicker()
{
	const bool bLastStep = FlickerStep >= StorySubsystemConst::FlickerSteps - 1;
	// Even steps (0, 2, ...) are "on"; the final step is even, so it settles on.
	const bool bOn = bLastStep || (FlickerStep % 2 == 0);

	for (int32 i = 0; i < DimmedLights.Num(); ++i)
	{
		if (ULightComponent* Light = DimmedLights[i].Get())
		{
			Light->SetIntensity(bOn ? DimmedOriginalIntensities[i] : 0.f);
		}
	}
	for (AActor* Actor : HiddenActors)
	{
		if (Actor)
		{
			if (USceneComponent* Root = Actor->GetRootComponent())
			{
				Root->SetVisibility(bOn, /*bPropagateToChildren*/ true);
			}
		}
	}

	++FlickerStep;
	if (bLastStep)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FlickerTimer);
		}
		UE_LOG(LogTemp, Log, TEXT("StorySubsystem: flicker complete — station lights restored"));
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
