#include "FootstepComponent.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EngineUtils.h"
#include "GameFramework/Volume.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Tunable.h"

WP_TUNABLE_FLOAT(GFootstepInterval, "weird.Footstep.Interval", 0.6f,
	"Seconds between footstep sounds while moving (fixed, metronome-like rhythm).");
WP_TUNABLE_FLOAT(GFootstepVolume, "weird.Footstep.Volume", 3.0f,
	"Footstep sound volume multiplier.");
WP_TUNABLE_FLOAT(GFootstepPitchJitter, "weird.Footstep.PitchJitter", 0.08f,
	"Random pitch variance (+/-) applied per step for variety (default for sets without an override).");

// Per-set overrides: "Set:pitch=<base>,jitter=<var>,vol=<mult>,interval=<sec>;Set2:..."
// — any key may be omitted. E.g. "Carpet:pitch=0.9,vol=1.25". Live-tunable.
static TAutoConsoleVariable<FString> CVarFootstepSetTuning(
	TEXT("weird.Footstep.SetTuning"), TEXT("Carpet:pitch=0.9,jitter=0.08,vol=1.5;Tar:vol=0.5,pitch=0.8"),
	TEXT("Per-set footstep overrides: \"Set:pitch=1.0,jitter=0.08,vol=1.0,interval=0.6;Set2:...\""));

// Parse this set's entry out of weird.Footstep.SetTuning.
static void GetSetTuning(const FString& SetName, float& OutBasePitch, float& OutJitter, float& OutVolume, float& OutInterval)
{
	OutBasePitch = 1.f;
	OutJitter = GFootstepPitchJitter;
	OutVolume = 1.f;
	OutInterval = GFootstepInterval;
	TArray<FString> Entries;
	CVarFootstepSetTuning.GetValueOnGameThread().ParseIntoArray(Entries, TEXT(";"));
	for (const FString& Entry : Entries)
	{
		FString Name, Params;
		if (!Entry.Split(TEXT(":"), &Name, &Params) || !Name.TrimStartAndEnd().Equals(SetName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		TArray<FString> Pairs;
		Params.ParseIntoArray(Pairs, TEXT(","));
		for (const FString& Pair : Pairs)
		{
			FString Key, Value;
			if (Pair.Split(TEXT("="), &Key, &Value))
			{
				Key = Key.TrimStartAndEnd();
				if (Key == TEXT("pitch")) { OutBasePitch = FCString::Atof(*Value); }
				else if (Key == TEXT("jitter")) { OutJitter = FCString::Atof(*Value); }
				else if (Key == TEXT("vol")) { OutVolume = FCString::Atof(*Value); }
				else if (Key == TEXT("interval")) { OutInterval = FCString::Atof(*Value); }
			}
		}
		return;
	}
}
WP_TUNABLE_INT(GFootstepSet, "weird.Footstep.Set", 1,
	"Fallback footstep set index (subfolders of /Game/Sounds/Footsteps, sorted) for floors without a Footstep.<SetName> actor tag.");

static const FString FootstepTagPrefix = TEXT("Footstep.");

void UFootstepComponent::UpdateSetTuning(const FString& SetName, const FString& Key, float Value)
{
	TArray<FString> Entries;
	CVarFootstepSetTuning.GetValueOnGameThread().ParseIntoArray(Entries, TEXT(";"));

	bool bFoundSet = false;
	for (FString& Entry : Entries)
	{
		FString Name, Params;
		if (!Entry.Split(TEXT(":"), &Name, &Params) || !Name.TrimStartAndEnd().Equals(SetName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		bFoundSet = true;
		TArray<FString> Pairs;
		Params.ParseIntoArray(Pairs, TEXT(","));
		bool bFoundKey = false;
		for (FString& Pair : Pairs)
		{
			FString K, V;
			if (Pair.Split(TEXT("="), &K, &V) && K.TrimStartAndEnd().Equals(Key, ESearchCase::IgnoreCase))
			{
				Pair = FString::Printf(TEXT("%s=%g"), *Key, Value);
				bFoundKey = true;
			}
		}
		if (!bFoundKey)
		{
			Pairs.Add(FString::Printf(TEXT("%s=%g"), *Key, Value));
		}
		Entry = Name + TEXT(":") + FString::Join(Pairs, TEXT(","));
	}
	if (!bFoundSet)
	{
		Entries.Add(FString::Printf(TEXT("%s:%s=%g"), *SetName, *Key, Value));
	}

	const FString NewValue = FString::Join(Entries, TEXT(";"));
	CVarFootstepSetTuning->Set(*NewValue, ECVF_SetByConsole);
	UE_LOG(LogTemp, Display, TEXT("FootstepComponent: SetTuning = %s"), *NewValue);
}

UFootstepComponent::UFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFootstepComponent::BeginPlay()
{
	Super::BeginPlay();

	// Load every set (= subfolder of /Game/Sounds/Footsteps) up front so the
	// per-step surface switch never hitches. Deleted folders linger as empty
	// paths in the asset registry — folders without sounds are skipped.
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FString> SubPaths;
	AssetRegistry.Get().GetSubPaths(TEXT("/Game/Sounds/Footsteps"), SubPaths, /*bInRecurse=*/false);
	SubPaths.Sort();

	for (const FString& SubPath : SubPaths)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.Get().GetAssetsByPath(FName(*SubPath), Assets, /*bRecursive=*/true);

		FFootstepSet Set;
		Set.Name = FPaths::GetPathLeaf(SubPath);
		for (const FAssetData& Asset : Assets)
		{
			if (USoundBase* Sound = Cast<USoundBase>(Asset.GetAsset()))
			{
				Set.Sounds.Add(Sound);
				KeepAliveSounds.Add(Sound);
			}
		}
		if (Set.Sounds.Num() > 0)
		{
			Sets.Add(MoveTemp(Set));
		}
	}
	LastSoundIndexPerSet.Init(INDEX_NONE, Sets.Num());

	// Collect Footstep.*-tagged volumes (e.g. the AV_* audio volumes) once.
	for (TActorIterator<AVolume> It(GetWorld()); It; ++It)
	{
		if (SetIndexFromTags(*It) != INDEX_NONE)
		{
			TaggedVolumes.Add(*It);
			UE_LOG(LogTemp, Display, TEXT("FootstepComponent: surface volume %s"), *It->GetActorNameOrLabel());
		}
	}

	if (Sets.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FootstepComponent: no sound sets under /Game/Sounds/Footsteps — footsteps will be silent."));
		return;
	}
	for (int32 i = 0; i < Sets.Num(); ++i)
	{
		UE_LOG(LogTemp, Display, TEXT("FootstepComponent: set %d = %s (%d sounds), tag \"Footstep.%s\""),
			i, *Sets[i].Name, Sets[i].Sounds.Num(), *Sets[i].Name);
	}
}

void UFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || Sets.Num() == 0)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bWalking = Movement && Movement->IsMovingOnGround() && Movement->Velocity.SizeSquared2D() > 1.f;

	// Fixed, metronome-like rhythm: a step every interval while moving (the
	// current surface's set can override the global interval), first step
	// immediately on starting to walk.
	float BasePitch, Jitter, SetVolume, Interval;
	GetSetTuning(Sets[ResolveSetIndex()].Name, BasePitch, Jitter, SetVolume, Interval);
	if (bWalking)
	{
		TimeSinceLastStep += DeltaTime;
		if (!bWasWalking || TimeSinceLastStep >= Interval)
		{
			TimeSinceLastStep = 0.f;
			PlayFootstep();
		}
	}
	else
	{
		// Keep partial progress so a stop-start doesn't fire instantly.
		TimeSinceLastStep = FMath::Min(TimeSinceLastStep, Interval * 0.5f);
	}
	bWasWalking = bWalking;
}

int32 UFootstepComponent::SetIndexFromTags(const AActor* Actor) const
{
	for (const FName& Tag : Actor->Tags)
	{
		const FString TagStr = Tag.ToString();
		if (TagStr.StartsWith(FootstepTagPrefix))
		{
			const FString SetName = TagStr.RightChop(FootstepTagPrefix.Len());
			for (int32 i = 0; i < Sets.Num(); ++i)
			{
				if (Sets[i].Name.Equals(SetName, ESearchCase::IgnoreCase))
				{
					return i;
				}
			}
			UE_LOG(LogTemp, Warning, TEXT("FootstepComponent: actor %s has tag %s but no set named %s exists"),
				*Actor->GetName(), *TagStr, *SetName);
		}
	}
	return INDEX_NONE;
}

int32 UFootstepComponent::ResolveSetIndex() const
{
	const int32 Fallback = FMath::Clamp(GFootstepSet, 0, Sets.Num() - 1);

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const FVector Feet = Character->GetActorLocation()
		- FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());

	// Tagged volumes take priority — they carve out regions (e.g. the bathroom)
	// inside a floor mesh that carries its own tag.
	for (const TObjectPtr<AActor>& VolumeActor : TaggedVolumes)
	{
		const AVolume* Volume = Cast<AVolume>(VolumeActor);
		if (Volume && Volume->EncompassesPoint(Feet))
		{
			LastFloorActor = const_cast<AVolume*>(Volume);
			return SetIndexFromTags(Volume);
		}
	}

	const FVector Start = Character->GetActorLocation();
	const FVector End = Feet - FVector(0.f, 0.f, 100.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FootstepSurface), /*bTraceComplex=*/false, Character);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) || !Hit.GetActor())
	{
		LastFloorActor = nullptr;
		return Fallback;
	}
	LastFloorActor = Hit.GetActor();

	const int32 Tagged = SetIndexFromTags(Hit.GetActor());
	return Tagged != INDEX_NONE ? Tagged : Fallback;
}

void UFootstepComponent::PlayFootstep()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const int32 SetIndex = ResolveSetIndex();
	const FFootstepSet& Set = Sets[SetIndex];

	// Pick a random variant, avoiding an immediate repeat when we have >1.
	int32 Index = FMath::RandRange(0, Set.Sounds.Num() - 1);
	if (Set.Sounds.Num() > 1 && Index == LastSoundIndexPerSet[SetIndex])
	{
		Index = (Index + 1) % Set.Sounds.Num();
	}
	LastSoundIndexPerSet[SetIndex] = Index;

	const FVector FootLocation = Character->GetActorLocation()
		- FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());
	float BasePitch, Jitter, SetVolume, Interval;
	GetSetTuning(Set.Name, BasePitch, Jitter, SetVolume, Interval);
	const float Pitch = BasePitch * (1.f + FMath::FRandRange(-Jitter, Jitter));
	UGameplayStatics::PlaySoundAtLocation(this, Set.Sounds[Index], FootLocation, GFootstepVolume * SetVolume, Pitch);
	UE_LOG(LogTemp, Display, TEXT("FootstepComponent: played %s/%s (floor: %s)"),
		*Set.Name, *Set.Sounds[Index]->GetName(), *GetNameSafe(LastFloorActor.Get()));
}
