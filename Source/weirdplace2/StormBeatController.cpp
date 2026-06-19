#include "StormBeatController.h"

#include "StorySubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "Sound/AmbientSound.h"

AStormBeatController::AStormBeatController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AStormBeatController::BeginPlay()
{
	Super::BeginPlay();

	UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr;
	if (!Story)
	{
		UE_LOG(LogTemp, Error, TEXT("AStormBeatController %s: no UStorySubsystem; storm beat will never fire"), *GetName());
		return;
	}

	FlagChangedHandle = Story->OnStoryFlagChanged.AddUObject(this, &AStormBeatController::OnStoryFlagChanged);

	// If the warning already showed (e.g. SkipTo dev command ran first), apply now.
	if (Story->IsFlagSet(EStoryFlag::TornadoWarningDisplayed))
	{
		ApplyStorm();
	}
}

void AStormBeatController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FlagChangedHandle.IsValid())
	{
		if (UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr)
		{
			Story->OnStoryFlagChanged.Remove(FlagChangedHandle);
		}
		FlagChangedHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void AStormBeatController::ConfigureForTest(const TArray<AActor*>& InLights, const TArray<AAmbientSound*>& InAmbients, float InMultiplier)
{
	LightsToDim = InLights;
	AmbientSoundsToSilence = InAmbients;
	DimMultiplier = InMultiplier;
}

void AStormBeatController::OnStoryFlagChanged(EStoryFlag Flag, bool bValue)
{
	if (Flag == EStoryFlag::TornadoWarningDisplayed && bValue)
	{
		ApplyStorm();
	}
}

void AStormBeatController::ApplyStorm()
{
	if (bApplied)
	{
		return;
	}
	bApplied = true;

	// Dim every light component on each referenced actor. Multiplying the current
	// intensity (rather than setting an absolute) keeps it unit-agnostic — lumens,
	// candelas, unitless — and leaves it dimmed; nothing restores it.
	int32 DimmedLights = 0;
	for (AActor* Actor : LightsToDim)
	{
		if (!Actor)
		{
			continue;
		}
		TArray<ULightComponent*> LightComps;
		Actor->GetComponents<ULightComponent>(LightComps);
		for (ULightComponent* Light : LightComps)
		{
			Light->SetIntensity(Light->Intensity * DimMultiplier);
			++DimmedLights;
		}
	}

	// Cut the store's TV ambient beds.
	int32 SilencedBeds = 0;
	for (AAmbientSound* Ambient : AmbientSoundsToSilence)
	{
		if (!Ambient)
		{
			continue;
		}
		if (UAudioComponent* AudioComp = Ambient->GetAudioComponent())
		{
			AudioComp->Stop();
			++SilencedBeds;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AStormBeatController %s: storm applied — dimmed %d light(s) x%.2f, silenced %d ambient bed(s)"),
		*GetName(), DimmedLights, DimMultiplier, SilencedBeds);
}
