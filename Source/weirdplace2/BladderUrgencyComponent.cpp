#include "BladderUrgencyComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

namespace BladderUrgencyInternal
{
	static const FName IntensityParamName(TEXT("Intensity"));
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
	SetVignetteIntensity(0.f);

	if (!UrgencySound)
	{
		UrgencySound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/bladder2.bladder2"));
	}

	StartTimeSeconds = GetWorld()->GetTimeSeconds();
	ScheduleNextPulse();
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

void UBladderUrgencyComponent::ScheduleNextPulse()
{
	const float Elapsed = GetWorld()->GetTimeSeconds() - StartTimeSeconds;
	const float Progress = FMath::Clamp(Elapsed / TimeUntilDeath, 0.f, 1.f);
	const float Interval = FMath::Lerp(StartInterval, MinInterval, Progress);

	GetWorld()->GetTimerManager().SetTimer(
		ReminderTimerHandle,
		this,
		&UBladderUrgencyComponent::StartPulse,
		Interval,
		false
	);
}

void UBladderUrgencyComponent::StartPulse()
{
	const float Elapsed = GetWorld()->GetTimeSeconds() - StartTimeSeconds;

	if (Elapsed >= TimeUntilDeath)
	{
		UE_LOG(LogTemp, Warning, TEXT("BladderUrgencyComponent: Time's up — bladder death on %s"), *GetOwner()->GetName());
		OnBladderDeath.Broadcast();
		return;
	}

	bIsPulsing = true;
	PulseElapsed = 0.f;
	SetComponentTickEnabled(true);

	if (UrgencySound)
	{
		UGameplayStatics::PlaySound2D(this, UrgencySound);
	}

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
