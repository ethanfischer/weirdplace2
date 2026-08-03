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

void UStormFogComponent::StartFog()
{
	if (bRamping || !FogMID || !CachedCamera)
	{
		return;
	}
	// Restore the real distance in case the warmup timer hasn't fired yet.
	FogMID->SetScalarParameterValue(StormFogInternal::FogDistanceParam, FogDistance);
	FogMID->SetVectorParameterValue(StormFogInternal::FogColorParam, FogColor);
	FogMID->SetScalarParameterValue(StormFogInternal::FogMaxOpacityParam, FogMaxOpacity);

	bRamping = true;
	RampElapsed = 0.f;

	UE_LOG(LogTemp, Log, TEXT("UStormFogComponent: pea-soup fog rolling in over %.1fs (FogDistance %.0f)"), RampDuration, FogDistance);

	if (RampDuration <= 0.f)
	{
		SetFogWeight(1.f);
		bRamping = false;
		return;
	}
	SetComponentTickEnabled(true);
}

void UStormFogComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bRamping)
	{
		RampElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(RampElapsed / RampDuration, 0.f, 1.f);
		SetFogWeight(Alpha);
		if (Alpha >= 1.f)
		{
			bRamping = false;
			UE_LOG(LogTemp, Log, TEXT("UStormFogComponent: pea-soup fog fully rolled in"));
		}
	}

	// While the fog is visible, keep the MID in sync with the UPROPERTYs so editing
	// FogDistance/FogColor on the component in the PIE Details panel updates live.
	if (FogMID && CurrentWeight > 0.f)
	{
		FogMID->SetScalarParameterValue(StormFogInternal::FogDistanceParam, FogDistance);
		FogMID->SetVectorParameterValue(StormFogInternal::FogColorParam, FogColor);
		FogMID->SetScalarParameterValue(StormFogInternal::FogMaxOpacityParam, FogMaxOpacity);
	}

	// Nothing left to do once fully cleared and not ramping — stop ticking.
	if (!bRamping && CurrentWeight <= 0.f)
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
	bRamping = false;
	if (CurrentWeight > 0.f)
	{
		SetFogWeight(0.f);
		UE_LOG(LogTemp, Log, TEXT("UStormFogComponent: fog toggled OFF"));
	}
	else
	{
		FogMID->SetScalarParameterValue(StormFogInternal::FogDistanceParam, FogDistance);
		FogMID->SetVectorParameterValue(StormFogInternal::FogColorParam, FogColor);
		FogMID->SetScalarParameterValue(StormFogInternal::FogMaxOpacityParam, FogMaxOpacity);
		SetFogWeight(1.f);
		SetComponentTickEnabled(true); // keep syncing params for live tuning
		UE_LOG(LogTemp, Log, TEXT("UStormFogComponent: fog toggled ON (dist %.0f)"), FogDistance);
	}
}

void UStormFogComponent::SetFogWeight(float Weight)
{
	if (CachedCamera && FogMID)
	{
		CurrentWeight = FMath::Clamp(Weight, 0.f, 1.f);
		StormFogInternal::SetBlendableWeight(CachedCamera->PostProcessSettings, FogMID, CurrentWeight);
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
