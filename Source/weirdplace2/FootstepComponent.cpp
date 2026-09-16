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
	"Random pitch variance (+/-) applied per step for variety (default for sets without a per-surface override).");

// Per-surface overrides. One block per sound set (subfolder of
// /Game/Sounds/Footsteps); a set with no block uses the global defaults above
// (pitch 1.0, the global jitter, volume 1.0). Add a block when you add a set.
WP_TUNABLE_FLOAT(GFootstepCarpetPitch,  "weird.Footstep.Carpet.Pitch",  0.9f,  "Carpet base pitch multiplier.");
WP_TUNABLE_FLOAT(GFootstepCarpetJitter, "weird.Footstep.Carpet.Jitter", 0.08f, "Carpet random pitch variance (+/-).");
WP_TUNABLE_FLOAT(GFootstepCarpetVolume, "weird.Footstep.Carpet.Volume", 1.5f,  "Carpet volume multiplier.");
WP_TUNABLE_FLOAT(GFootstepTarPitch,     "weird.Footstep.Tar.Pitch",     0.8f,  "Tar base pitch multiplier.");
WP_TUNABLE_FLOAT(GFootstepTarJitter,    "weird.Footstep.Tar.Jitter",    0.08f, "Tar random pitch variance (+/-).");
WP_TUNABLE_FLOAT(GFootstepTarVolume,    "weird.Footstep.Tar.Volume",    0.5f,  "Tar volume multiplier.");

// This set's base pitch / jitter / volume, falling back to the globals for a
// set with no per-surface block above.
static void GetSetTuning(const FString& SetName, float& OutBasePitch, float& OutJitter, float& OutVolume)
{
	OutBasePitch = 1.f;
	OutJitter = GFootstepPitchJitter;
	OutVolume = 1.f;
	if (SetName.Equals(TEXT("Carpet"), ESearchCase::IgnoreCase))
	{
		OutBasePitch = GFootstepCarpetPitch;
		OutJitter = GFootstepCarpetJitter;
		OutVolume = GFootstepCarpetVolume;
	}
	else if (SetName.Equals(TEXT("Tar"), ESearchCase::IgnoreCase))
	{
		OutBasePitch = GFootstepTarPitch;
		OutJitter = GFootstepTarJitter;
		OutVolume = GFootstepTarVolume;
	}
}
WP_TUNABLE_INT(GFootstepSet, "weird.Footstep.Set", 1,
	"Fallback footstep set index (subfolders of /Game/Sounds/Footsteps, sorted) for floors without a Footstep.<SetName> actor tag.");

static const FString FootstepTagPrefix = TEXT("Footstep.");

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
	if (bWalking)
	{
		TimeSinceLastStep += DeltaTime;
		if (!bWasWalking || TimeSinceLastStep >= GFootstepInterval)
		{
			TimeSinceLastStep = 0.f;
			PlayFootstep();
		}
	}
	else
	{
		// Keep partial progress so a stop-start doesn't fire instantly.
		TimeSinceLastStep = FMath::Min(TimeSinceLastStep, GFootstepInterval * 0.5f);
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
	float BasePitch, Jitter, SetVolume;
	GetSetTuning(Set.Name, BasePitch, Jitter, SetVolume);
	const float Pitch = BasePitch * (1.f + FMath::FRandRange(-Jitter, Jitter));
	UGameplayStatics::PlaySoundAtLocation(this, Set.Sounds[Index], FootLocation, GFootstepVolume * SetVolume, Pitch);
	UE_LOG(LogTemp, Display, TEXT("FootstepComponent: played %s/%s (floor: %s)"),
		*Set.Name, *Set.Sounds[Index]->GetName(), *GetNameSafe(LastFloorActor.Get()));
}
