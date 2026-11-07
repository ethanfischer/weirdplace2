#include "BladderUrgencyComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Tunable.h"

WP_TUNABLE_FLOAT(GBladderMinInterval, "weird.Bladder.MinInterval", 10.f,
	"Fastest gap between bladder alerts (seconds), reached right before death. Alerts start StartInterval apart and close in to this.");
WP_TUNABLE_FLOAT(GBladderStartCoverage, "weird.Bladder.StartCoverage", 3.f,
	"Yellow vignette spread on the first alert (M_BladderVignette Coverage: 3 = corners/edges only).");
WP_TUNABLE_FLOAT(GBladderEndCoverage, "weird.Bladder.EndCoverage", 10.f,
	"Yellow vignette spread right before death — higher reaches further into the screen.");

namespace BladderUrgencyInternal
{
	static const FName IntensityParamName(TEXT("Intensity"));
	static const FName CoverageParamName(TEXT("Coverage"));
	static const TCHAR* DefaultVignetteMaterialPath = TEXT("/Game/CreatedMaterials/M_BladderVignette.M_BladderVignette");

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

		for (int32 Index = WeightedBlendables.Array.Num() - 1; Index >= 0; --Index)
		{
			if (Index != FoundIndex && WeightedBlendables.Array[Index].Object == BlendableObject)
			{
				WeightedBlendables.Array.RemoveAt(Index);
			}
		}
	}
}

UBladderUrgencyComponent::UBladderUrgencyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UBladderUrgencyComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCamera = GetOwner()->FindComponentByClass<UCameraComponent>();
	if (!CachedCamera)
	{
		UE_LOG(LogTemp, Error, TEXT("BladderUrgencyComponent: No UCameraComponent found on %s"), *GetOwner()->GetName());
		return;
	}

	ResetLegacyPostProcessOverrides();
	InitializeVignetteMaterial();

	// Warm the vignette's post-process pipeline at load: a weight-0 blendable
	// is culled from rendering, so its shaders otherwise compile at the FIRST
	// real pulse — which renders garbage exactly when the player is meant to
	// see it (verified live 2026-07-02: first render draws a corrupt border,
	// correct from the second render on). Hold a just-above-threshold weight
	// for the first second instead, then drop to 0. Weights below ~1/255
	// (e.g. the 0.002 tried first) are culled and never compile.
	SetVignetteIntensity(0.05f);
	GetWorld()->GetTimerManager().SetTimer(
		WarmupTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]() { SetVignetteIntensity(0.f); }),
		1.0f, false);

	if (!UrgencySound)
	{
		UrgencySound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/bladder2.bladder2"));
	}

	if (bAutoStart)
	{
		StartUrgency();
	}
}

void UBladderUrgencyComponent::StartUrgency()
{
	StartTimeSeconds = GetWorld()->GetTimeSeconds();
	ScheduleNextPulse();

	GetWorld()->GetTimerManager().SetTimer(
		DeathTimerHandle, this, &UBladderUrgencyComponent::OnBladderDeathFired,
		TimeUntilDeath, false);
}

bool UBladderUrgencyComponent::InitializeVignetteMaterial()
{
	if (!CachedCamera)
	{
		return false;
	}

	if (!UrgencyVignetteMaterial)
	{
		UrgencyVignetteMaterial = Cast<UMaterialInterface>(StaticLoadObject(
			UMaterialInterface::StaticClass(),
			nullptr,
			BladderUrgencyInternal::DefaultVignetteMaterialPath));
	}

	if (!UrgencyVignetteMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("BladderUrgencyComponent: Missing urgency vignette material on %s"), *GetOwner()->GetName());
		return false;
	}

	UrgencyVignetteMID = UMaterialInstanceDynamic::Create(UrgencyVignetteMaterial, this);
	if (!UrgencyVignetteMID)
	{
		UE_LOG(LogTemp, Warning, TEXT("BladderUrgencyComponent: Failed to create MID for %s"), *GetOwner()->GetName());
		return false;
	}

	UrgencyVignetteMID->SetScalarParameterValue(BladderUrgencyInternal::IntensityParamName, 1.f);
	BladderUrgencyInternal::SetBlendableWeight(CachedCamera->PostProcessSettings, UrgencyVignetteMID, 0.f);
	return true;
}

void UBladderUrgencyComponent::SetVignetteIntensity(float Value)
{
	if (!CachedCamera || !UrgencyVignetteMID)
	{
		return;
	}

	const float ClampedValue = FMath::Clamp(Value, 0.f, 1.f);
	// Drive fade through one stable blendable entry.
	BladderUrgencyInternal::SetBlendableWeight(CachedCamera->PostProcessSettings, UrgencyVignetteMID, ClampedValue);

	// Keep parameter in sync as a secondary path.
	UrgencyVignetteMID->SetScalarParameterValue(BladderUrgencyInternal::IntensityParamName, ClampedValue);
}

void UBladderUrgencyComponent::ResetLegacyPostProcessOverrides()
{
	if (!CachedCamera)
	{
		return;
	}

	CachedCamera->PostProcessSettings.bOverride_VignetteIntensity = false;
	CachedCamera->PostProcessSettings.bOverride_ColorSaturation = false;
	CachedCamera->PostProcessSettings.bOverride_ColorGain = false;
	CachedCamera->PostProcessSettings.VignetteIntensity = 0.f;
	CachedCamera->PostProcessSettings.ColorSaturation = FVector4(1.f, 1.f, 1.f, 1.f);
	CachedCamera->PostProcessSettings.ColorGain = FVector4(1.f, 1.f, 1.f, 1.f);
}

void UBladderUrgencyComponent::FireSinglePulse()
{
	bIsPulsing = true;
	PulseElapsed = 0.f;
	SetComponentTickEnabled(true);

	// Each alert reaches further in from the edges than the last.
	if (UrgencyVignetteMID)
	{
		UrgencyVignetteMID->SetScalarParameterValue(BladderUrgencyInternal::CoverageParamName,
			FMath::Lerp(GBladderStartCoverage, GBladderEndCoverage, GetUrgencyProgress()));
	}

	if (UrgencySound)
	{
		UGameplayStatics::PlaySound2D(this, UrgencySound);
	}
}

float UBladderUrgencyComponent::GetUrgencyProgress() const
{
	// Before StartUrgency (the car ride's one-off pulse) there's no countdown yet.
	if (!GetWorld()->GetTimerManager().IsTimerActive(DeathTimerHandle))
	{
		return 0.f;
	}
	const float Elapsed = GetWorld()->GetTimeSeconds() - StartTimeSeconds;
	return FMath::Clamp(Elapsed / TimeUntilDeath, 0.f, 1.f);
}

void UBladderUrgencyComponent::ScheduleNextPulse()
{
	const float Interval = FMath::Lerp(StartInterval, GBladderMinInterval, GetUrgencyProgress());

	GetWorld()->GetTimerManager().SetTimer(
		ReminderTimerHandle,
		this,
		&UBladderUrgencyComponent::StartPulse,
		Interval,
		false
	);
}

void UBladderUrgencyComponent::OnBladderDeathFired()
{
	UE_LOG(LogTemp, Warning, TEXT("BladderUrgencyComponent: Time's up — bladder death on %s"), *GetOwner()->GetName());
	OnBladderDeath.Broadcast();
}

void UBladderUrgencyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ReminderTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(DeathTimerHandle);
	if (CachedCamera && UrgencyVignetteMID)
	{
		BladderUrgencyInternal::SetBlendableWeight(CachedCamera->PostProcessSettings, UrgencyVignetteMID, 0.f);
		CachedCamera->PostProcessSettings.WeightedBlendables.Array.RemoveAll(
			[this](const FWeightedBlendable& B){ return B.Object == UrgencyVignetteMID; });
		UrgencyVignetteMID = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UBladderUrgencyComponent::StopUrgency()
{
	UE_LOG(LogTemp, Display, TEXT("BladderUrgencyComponent: StopUrgency on %s"), *GetOwner()->GetName());

	GetWorld()->GetTimerManager().ClearTimer(ReminderTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(DeathTimerHandle);

	bIsPulsing = false;
	SetComponentTickEnabled(false);

	if (CachedCamera && UrgencyVignetteMID)
	{
		BladderUrgencyInternal::SetBlendableWeight(CachedCamera->PostProcessSettings, UrgencyVignetteMID, 0.f);
		CachedCamera->PostProcessSettings.WeightedBlendables.Array.RemoveAll(
			[this](const FWeightedBlendable& B){ return B.Object == UrgencyVignetteMID; });
		UrgencyVignetteMID = nullptr;
	}
}

void UBladderUrgencyComponent::StartPulse()
{
	FireSinglePulse();
	ScheduleNextPulse();
}

void UBladderUrgencyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsPulsing || !CachedCamera)
	{
		return;
	}

	PulseElapsed += DeltaTime;

	const float SafeDuration = FMath::Max(PulseDuration, 0.01f);
	if (PulseElapsed >= SafeDuration)
	{
		SetVignetteIntensity(0.f);
		bIsPulsing = false;
		SetComponentTickEnabled(false);
		return;
	}

	const float NormalizedTime = FMath::Clamp(PulseElapsed / SafeDuration, 0.f, 1.f);
	// Cosine bell: 0 -> 1 -> 0 with smooth start/end (no hard pop on first/last frames).
	const float Alpha = 0.5f - 0.5f * FMath::Cos(2.f * PI * NormalizedTime);
	const float IntensityScale = FMath::Clamp(PulseIntensity, 0.f, 1.f);
	const float Intensity = FMath::Clamp(Alpha * IntensityScale, 0.f, 1.f);
	SetVignetteIntensity(Intensity);
}
