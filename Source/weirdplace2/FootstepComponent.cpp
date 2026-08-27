#include "FootstepComponent.h"

#include "AssetRegistry/AssetRegistryModule.h"
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
	"Random pitch variance (+/-) applied per step for variety.");
WP_TUNABLE_INT(GFootstepSet, "weird.Footstep.Set", 0,
	"Which footstep sound set (subfolder of /Game/Sounds/Footsteps, sorted alphabetically) to use. Switchable live.");

UFootstepComponent::UFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFootstepComponent::BeginPlay()
{
	Super::BeginPlay();

	// Each subfolder of /Game/Sounds/Footsteps is one selectable sound set.
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FString> SubPaths;
	AssetRegistry.Get().GetSubPaths(TEXT("/Game/Sounds/Footsteps"), SubPaths, /*bInRecurse=*/false);
	SubPaths.Sort();
	// Deleted folders linger as empty paths in the asset registry — skip them.
	SetPaths.Reset();
	for (const FString& SubPath : SubPaths)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.Get().GetAssetsByPath(FName(*SubPath), Assets, /*bRecursive=*/true);
		if (Assets.Num() > 0)
		{
			SetPaths.Add(SubPath);
		}
	}

	if (SetPaths.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FootstepComponent: no set folders under /Game/Sounds/Footsteps — footsteps will be silent."));
		return;
	}
	for (int32 i = 0; i < SetPaths.Num(); ++i)
	{
		UE_LOG(LogTemp, Display, TEXT("FootstepComponent: weird.Footstep.Set %d = %s"), i, *SetPaths[i]);
	}
	LoadSet(FMath::Clamp(GFootstepSet, 0, SetPaths.Num() - 1));
}

void UFootstepComponent::LoadSet(int32 SetIndex)
{
	FootstepSounds.Reset();
	LoadedSetIndex = SetIndex;

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssetsByPath(FName(*SetPaths[SetIndex]), Assets, /*bRecursive=*/true);

	for (const FAssetData& Asset : Assets)
	{
		if (USoundBase* Sound = Cast<USoundBase>(Asset.GetAsset()))
		{
			FootstepSounds.Add(Sound);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FootstepComponent: loaded set %d (%s), %d sound(s)"),
		SetIndex, *SetPaths[SetIndex], FootstepSounds.Num());
	if (FootstepSounds.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FootstepComponent: set %s has no sounds — footsteps will be silent."), *SetPaths[SetIndex]);
	}
}

void UFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Live A/B: reload when the weird.Footstep.Set cvar changes.
	if (SetPaths.Num() > 0)
	{
		const int32 WantedSet = FMath::Clamp(GFootstepSet, 0, SetPaths.Num() - 1);
		if (WantedSet != LoadedSetIndex)
		{
			LoadSet(WantedSet);
		}
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || FootstepSounds.Num() == 0)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bWalking = Movement && Movement->IsMovingOnGround() && Movement->Velocity.SizeSquared2D() > 1.f;

	// Fixed, metronome-like rhythm: a step every GFootstepInterval seconds while
	// moving, first step immediately on starting to walk.
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

void UFootstepComponent::PlayFootstep()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());

	// Pick a random variant, avoiding an immediate repeat when we have >1.
	int32 Index = FMath::RandRange(0, FootstepSounds.Num() - 1);
	if (FootstepSounds.Num() > 1 && Index == LastSoundIndex)
	{
		Index = (Index + 1) % FootstepSounds.Num();
	}
	LastSoundIndex = Index;

	const FVector FootLocation = Character->GetActorLocation()
		- FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());
	const float Pitch = 1.f + FMath::FRandRange(-GFootstepPitchJitter, GFootstepPitchJitter);
	UGameplayStatics::PlaySoundAtLocation(this, FootstepSounds[Index], FootLocation, GFootstepVolume, Pitch);
	UE_LOG(LogTemp, Display, TEXT("FootstepComponent: played %s"), *FootstepSounds[Index]->GetName());
}
