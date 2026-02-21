#include "BladderUrgencyComponent.h"
#include "Camera/CameraComponent.h"

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

	GetWorld()->GetTimerManager().SetTimer(
		ReminderTimerHandle,
		this,
		&UBladderUrgencyComponent::StartPulse,
		ReminderInterval,
		true
	);
}

void UBladderUrgencyComponent::StartPulse()
{
	bIsPulsing = true;
	PulseElapsed = 0.f;
	SetComponentTickEnabled(true);
}

void UBladderUrgencyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsPulsing || !CachedCamera)
	{
		return;
	}

	PulseElapsed += DeltaTime;

	if (PulseElapsed >= PulseDuration)
	{
		// Reset all overrides
		CachedCamera->PostProcessSettings.bOverride_VignetteIntensity = false;
		CachedCamera->PostProcessSettings.bOverride_SceneColorTint = false;
		CachedCamera->PostProcessSettings.SceneColorTint = FLinearColor::White;
		CachedCamera->PostProcessSettings.VignetteIntensity = 0.f;
		bIsPulsing = false;
		SetComponentTickEnabled(false);
		return;
	}

	// Sine curve: 0 → 1 → 0 over PulseDuration
	const float Alpha = FMath::Sin((PulseElapsed / PulseDuration) * PI);

	// Dark vignette at edges
	CachedCamera->PostProcessSettings.bOverride_VignetteIntensity = true;
	CachedCamera->PostProcessSettings.VignetteIntensity = VignetteMax * Alpha;

	// Yellow tint (reduce blue channel)
	const float BlueDim = 1.f - (TintStrength * Alpha);
	CachedCamera->PostProcessSettings.bOverride_SceneColorTint = true;
	CachedCamera->PostProcessSettings.SceneColorTint = FLinearColor(1.f, 1.f, BlueDim, 1.f);
}
