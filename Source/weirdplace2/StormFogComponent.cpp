#include "StormFogComponent.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "StorySubsystem.h"
#include "TimerManager.h"

namespace StormFogInternal
{
	static const FName FogColorParam(TEXT("FogColor"));
	static const FName FogDistanceParam(TEXT("FogDistance"));
	static const FName FogMaxOpacityParam(TEXT("FogMaxOpacity"));
	static const TCHAR* FogMaterialPath = TEXT("/Game/CreatedMaterials/M_PeaSoupFog.M_PeaSoupFog");

	// Warmup weight: below ~1/255 a blendable is culled and never compiles its
	// shaders, so the FIRST real frame renders garbage (see UBladderUrgencyComponent).
	// 0.06 is above the cull threshold; paired with a huge warmup FogDistance the
	// pass is effectively invisible while it compiles.
	static constexpr float WarmupWeight = 0.06f;
	static constexpr float WarmupFogDistance = 1.0e12f; // fogFactor ~= 0 everywhere

	// Add/update a single stable blendable entry (copied from UBladderUrgencyComponent).
	static void SetBlendableWeight(FPostProcessSettings& PostProcessSettings, UObject* BlendableObject, float Weight)
	{
		if (!BlendableObject)
		{
			return;
		}

		FWeightedBlendables& WeightedBlendables = PostProcessSettings.WeightedBlendables;
		const float ClampedWeight = FMath::Clamp(Weight, 0.f, 1.f);
		int32 FoundIndex = INDEX_NONE;

		for (int32 Index = 0; Index < WeightedBlendables.Array.Num(); ++Index)
		{
			if (WeightedBlendables.Array[Index].Object == BlendableObject)
			{
				FoundIndex = Index;
				break;
			}
		}

		if (FoundIndex == INDEX_NONE)
		{
			WeightedBlendables.Array.Add(FWeightedBlendable(ClampedWeight, BlendableObject));
			return;
		}

		WeightedBlendables.Array[FoundIndex].Weight = ClampedWeight;
	}
}

UStormFogComponent::UStormFogComponent()
{
	// Ticks only while actively ramping (enabled in StartFog, disabled on finish).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UStormFogComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCamera = GetOwner()->FindComponentByClass<UCameraComponent>();
	if (!CachedCamera)
	{
		UE_LOG(LogTemp, Error, TEXT("UStormFogComponent: no UCameraComponent on %s; pea-soup fog disabled"), *GetOwner()->GetName());
		return;
	}

	FogMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, StormFogInternal::FogMaterialPath));
	if (!FogMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("UStormFogComponent: missing fog material at %s; pea-soup fog disabled"), StormFogInternal::FogMaterialPath);
		return;
	}

	FogMID = UMaterialInstanceDynamic::Create(FogMaterial, this);
	FogMID->SetVectorParameterValue(StormFogInternal::FogColorParam, FogColor);

	// Warm the post-process pipeline at load with an effectively-invisible pass
	// (huge FogDistance -> fogFactor ~= 0), then drop the weight to 0. Without this
	// the first socked-in frame renders corrupt.
	FogMID->SetScalarParameterValue(StormFogInternal::FogDistanceParam, StormFogInternal::WarmupFogDistance);
	StormFogInternal::SetBlendableWeight(CachedCamera->PostProcessSettings, FogMID, StormFogInternal::WarmupWeight);
	GetWorld()->GetTimerManager().SetTimer(
		WarmupTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (FogMID && CachedCamera)
			{
				FogMID->SetScalarParameterValue(StormFogInternal::FogDistanceParam, FogDistance);
				SetFogWeight(0.f);
			}
		}),
		1.0f, false);

	// Subscribe to the storm beat (mirrors AUltraDynamicWeatherController).
	if (UStorySubsystem* Story = GetWorld()->GetSubsystem<UStorySubsystem>())
	{
		FlagChangedHandle = Story->OnStoryFlagChanged.AddUObject(this, &UStormFogComponent::OnStoryFlagChanged);
		if (Story->IsFlagSet(EStoryFlag::SeenTornadoWarning))
		{
			StartFog();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UStormFogComponent: no UStorySubsystem; fog will never roll in"));
	}
}

void UStormFogComponent::OnStoryFlagChanged(EStoryFlag Flag, bool bValue)
{
	if (Flag == EStoryFlag::SeenTornadoWarning && bValue)
	{
		StartFog();
	}
}

void UStormFogComponent::BeginRamp(float Target, float Duration)
{
	if (!FogMID || !CachedCamera)
	{
		return;
	}
	RampFromAmount = FogAmount;
	RampToAmount = FMath::Clamp(Target, 0.f, 1.f);
	ActiveRampDuration = Duration;
	RampElapsed = 0.f;

	// Apply the current amount now so the blendable's first render is correct — when
	// starting from clear that's weight 0 (invisible), which hides any one-frame
	// material-default flash before the roll-in takes over.
	ApplyFog(FogAmount);

	if (Duration <= 0.f)
	{
		ApplyFog(RampToAmount);
		bRamping = false;
		SetComponentTickEnabled(FogAmount > 0.f); // keep ticking for live param sync only while visible
		return;
	}
	bRamping = true;
	SetComponentTickEnabled(true);
}

void UStormFogComponent::ApplyFog(float Amount)
{
	FogAmount = FMath::Clamp(Amount, 0.f, 1.f);
	if (!FogMID)
	{
		return;
	}
	// Smoothstep the amount, then geometrically interp the fog distance from the (clear)
	// start distance down to the settled distance. Geometric interp keeps the close-in
	// feeling even across the big range; smoothstep removes the start/stop jerk.
	const float Eased = FogAmount * FogAmount * (3.f - 2.f * FogAmount);
	const float Start = FMath::Max(FogStartDistance, FogDistance);
	const float End = FMath::Max(1.f, FogDistance);
	const float Dist = FMath::Exp(FMath::Lerp(FMath::Loge(Start), FMath::Loge(End), Eased));

	FogMID->SetScalarParameterValue(StormFogInternal::FogDistanceParam, Dist);
	FogMID->SetVectorParameterValue(StormFogInternal::FogColorParam, FogColor);
	FogMID->SetScalarParameterValue(StormFogInternal::FogMaxOpacityParam, FogMaxOpacity);

	// The distance closing in does the fading, so the blendable is simply on whenever
	// there's any fog and off when fully clear.
	SetFogWeight(FogAmount > 0.f ? 1.f : 0.f);
}

void UStormFogComponent::StartFog()
{
	if (bRamping || !FogMID || !CachedCamera)
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("UStormFogComponent: pea-soup fog rolling in over %.1fs (%.0f -> %.0f cm)"), RampDuration, FogStartDistance, FogDistance);
	BeginRamp(1.f, RampDuration);
}

void UStormFogComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bRamping)
	{
		RampElapsed += DeltaTime;
		const float T = FMath::Clamp(RampElapsed / ActiveRampDuration, 0.f, 1.f);
		ApplyFog(FMath::Lerp(RampFromAmount, RampToAmount, T));
		if (T >= 1.f)
		{
			bRamping = false;
		}
	}
	else if (FogAmount > 0.f)
	{
		// Settled and visible: re-apply so live FogDistance/FogColor/FogMaxOpacity edits
		// in the PIE Details panel take effect immediately.
		ApplyFog(FogAmount);
	}

	// Nothing left to do once fully cleared and not ramping — stop ticking.
	if (!bRamping && FogAmount <= 0.f)
	{
		SetComponentTickEnabled(false);
	}
}

void UStormFogComponent::ToggleFog()
{
	if (!FogMID || !CachedCamera)
	{
		return;
	}
	// "On" = visible or ramping up. Ease the distance in/out over a few seconds — never
	// a hard pop.
	const bool bOn = (FogAmount > 0.f) || (bRamping && RampToAmount > 0.f);
	BeginRamp(bOn ? 0.f : 1.f, 3.0f);
	UE_LOG(LogTemp, Log, TEXT("UStormFogComponent: fog toggled %s"), bOn ? TEXT("OFF") : TEXT("ON"));
}

void UStormFogComponent::SetFogWeight(float Weight)
{
	if (CachedCamera && FogMID)
	{
		StormFogInternal::SetBlendableWeight(CachedCamera->PostProcessSettings, FogMID, FMath::Clamp(Weight, 0.f, 1.f));
	}
}

void UStormFogComponent::RemoveBlendable()
{
	if (CachedCamera && FogMID)
	{
		CachedCamera->PostProcessSettings.WeightedBlendables.Array.RemoveAll(
			[this](const FWeightedBlendable& B) { return B.Object == FogMID; });
	}
}

void UStormFogComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarmupTimerHandle);
		if (FlagChangedHandle.IsValid())
		{
			if (UStorySubsystem* Story = World->GetSubsystem<UStorySubsystem>())
			{
				Story->OnStoryFlagChanged.Remove(FlagChangedHandle);
			}
			FlagChangedHandle.Reset();
		}
	}
	RemoveBlendable();
	FogMID = nullptr;
	Super::EndPlay(EndPlayReason);
}
