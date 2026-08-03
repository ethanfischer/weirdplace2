#include "UltraDynamicWeatherController.h"

#include "StorySubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Sound/AmbientSound.h"
#include "UObject/UnrealType.h"

namespace
{
	// Placed UDS actor classes are the Blueprint-generated "Ultra_Dynamic_Sky_C" and
	// "Ultra_Dynamic_Weather_C". Match by substring to avoid a hard dependency on the
	// UDS plugin types (same approach as ATeleportTriggerBox).
	const TCHAR* GSkyClassNeedle = TEXT("Ultra_Dynamic_Sky");
	const TCHAR* GWeatherClassNeedle = TEXT("Ultra_Dynamic_Weather");

	// UDS knobs (FNames carry literal spaces — that's how the Blueprint variables are
	// actually named, confirmed by reflection against the live actors).
	const FName GOverallIntensityProp(TEXT("Overall Intensity"));                 // Sky
	const FName GWindIntensityProp(TEXT("Wind Intensity"));                       // Weather

	// UDW only honours Wind Intensity while this override bool is set; otherwise the
	// float sits at a ~1e8 sentinel and wind comes from the weather state.
	const FName GWindManualOverrideProp(TEXT("Wind Intensity - Manual Override")); // Weather (bool)
	// Poked after each Wind Intensity write so UDW re-applies it (the BP setter's
	// delegate doesn't fire on a raw reflection write).
	const FName GWindUpdateNeededProp(TEXT("Wind Intensity Update Needed"));       // Weather (bool)

	// UDS "Fog" scalar drives overall fogginess. Like wind it lives on the weather
	// state, so UDW only honours a manual value once the override bool is set; and the
	// value must be flagged dirty so UDW re-pushes it to the height/volumetric fog.
	const FName GFogProp(TEXT("Fog"));                                            // Weather (float)
	const FName GFogManualOverrideProp(TEXT("Fog - Manual Override"));            // Weather (bool)
	const FName GFogUpdateNeededProp(TEXT("Fog Update Needed"));                  // Weather (bool)

	// Resolve a floating-point property + its value pointer on Actor. FNumericProperty
	// covers both float and double reals, so this is robust to UE5 promoting Blueprint
	// floats to doubles. Null on miss.
	FNumericProperty* ResolveFloat(AActor* Actor, const FName& Name, void*& OutValuePtr)
	{
		OutValuePtr = nullptr;
		if (!Actor)
		{
			return nullptr;
		}
		FNumericProperty* Num = CastField<FNumericProperty>(Actor->GetClass()->FindPropertyByName(Name));
		if (!Num || !Num->IsFloatingPoint())
		{
			return nullptr;
		}
		OutValuePtr = Num->ContainerPtrToValuePtr<void>(Actor);
		return Num;
	}

	bool ReadFloat(AActor* Actor, const FName& Name, float& OutValue)
	{
		void* ValuePtr = nullptr;
		FNumericProperty* Num = ResolveFloat(Actor, Name, ValuePtr);
		if (!Num)
		{
			return false;
		}
		OutValue = static_cast<float>(Num->GetFloatingPointPropertyValue(ValuePtr));
		return true;
	}

	bool WriteFloat(AActor* Actor, const FName& Name, float Value)
	{
		void* ValuePtr = nullptr;
		FNumericProperty* Num = ResolveFloat(Actor, Name, ValuePtr);
		if (!Num)
		{
			return false;
		}
		Num->SetFloatingPointPropertyValue(ValuePtr, static_cast<double>(Value));
		return true;
	}

	bool SetBool(AActor* Actor, const FName& Name, bool Value)
	{
		if (!Actor)
		{
			return false;
		}
		FBoolProperty* Bool = CastField<FBoolProperty>(Actor->GetClass()->FindPropertyByName(Name));
		if (!Bool)
		{
			return false;
		}
		Bool->SetPropertyValue_InContainer(Actor, Value);
		return true;
	}
}

AUltraDynamicWeatherController::AUltraDynamicWeatherController()
{
	// Ticks only while actively transitioning (enabled in BeginFade, disabled on finish).
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AUltraDynamicWeatherController::BeginPlay()
{
	Super::BeginPlay();

	UStorySubsystem* Story = GetWorld() ? GetWorld()->GetSubsystem<UStorySubsystem>() : nullptr;
	if (!Story)
	{
		UE_LOG(LogTemp, Error, TEXT("AUltraDynamicWeatherController %s: no UStorySubsystem; storm transition will never start"), *GetName());
		return;
	}

	FlagChangedHandle = Story->OnStoryFlagChanged.AddUObject(this, &AUltraDynamicWeatherController::OnStoryFlagChanged);

	// If the warning was already seen (e.g. SkipTo dev command ran first), start now.
	if (Story->IsFlagSet(EStoryFlag::SeenTornadoWarning))
	{
		BeginFade();
	}
}

void AUltraDynamicWeatherController::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void AUltraDynamicWeatherController::OnStoryFlagChanged(EStoryFlag Flag, bool bValue)
{
	if (Flag == EStoryFlag::SeenTornadoWarning && bValue)
	{
		BeginFade();
	}
}

void AUltraDynamicWeatherController::BeginFade()
{
	if (bFading)
	{
		return;
	}

	// Sky "Overall Intensity" channel.
	if (!SkyActor.IsValid())
	{
		SkyActor = FindActorByClassNeedle(GSkyClassNeedle);
	}
	bFadeSky = ReadFloat(SkyActor.Get(), GOverallIntensityProp, SkyStartIntensity);
	if (!bFadeSky)
	{
		UE_LOG(LogTemp, Error, TEXT("AUltraDynamicWeatherController %s: no Ultra_Dynamic_Sky / 'Overall Intensity'; sky fade skipped"), *GetName());
	}
	
	// Ambient wind "Volume Multiplier" channel. The wind ambient is a plain placed
	// AmbientSound (no distinctive runtime class/name), so it's a designer-assigned
	// reference. Capture its current volume as the ramp start.
	bLoudenWind = false;
	if (AmbientGlobalWind)
	{
		if (UAudioComponent* AudioComp = AmbientGlobalWind->GetAudioComponent())
		{
			WindVolumeStart = AudioComp->VolumeMultiplier;
			bLoudenWind = true;
		}
	}
	if (!bLoudenWind)
	{
		UE_LOG(LogTemp, Warning, TEXT("AUltraDynamicWeatherController %s: AmbientGlobalWind unassigned or has no AudioComponent; wind volume swell skipped"), *GetName());
	}

	// Weather "Wind Intensity" channel. Engage manual override (UDW ignores the value
	// otherwise) and seed the start value; ApplyAtAlpha drives it from here. All-or-
	// nothing so we never half-enable the override.
	if (!WeatherActor.IsValid())
	{
		WeatherActor = FindActorByClassNeedle(GWeatherClassNeedle);
	}
	bRampWind =
		SetBool(WeatherActor.Get(), GWindManualOverrideProp, true) &&
		WriteFloat(WeatherActor.Get(), GWindIntensityProp, StartWindIntensity);
	if (bRampWind)
	{
		SetBool(WeatherActor.Get(), GWindUpdateNeededProp, true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AUltraDynamicWeatherController %s: no Ultra_Dynamic_Weather / wind override props; wind ramp skipped"), *GetName());
	}

	// Weather "Fog" channel. Same manual-override machinery as wind (shares the already
	// resolved WeatherActor): engage the override so UDW stops driving fog from the
	// weather state, capture the current value as the ramp start, and flag it dirty.
	bRampFog =
		SetBool(WeatherActor.Get(), GFogManualOverrideProp, true) &&
		ReadFloat(WeatherActor.Get(), GFogProp, FogStart);
	if (bRampFog)
	{
		SetBool(WeatherActor.Get(), GFogUpdateNeededProp, true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AUltraDynamicWeatherController %s: no Ultra_Dynamic_Weather / fog override props; fog ramp skipped"), *GetName());
	}

	if (!bFadeSky && !bRampWind && !bRampFog)
	{
		UE_LOG(LogTemp, Error, TEXT("AUltraDynamicWeatherController %s: no UDS actors to drive; transition aborted"), *GetName());
		return;
	}

	bFading = true;
	FadeElapsed = 0.f;

	UE_LOG(LogTemp, Log, TEXT("AUltraDynamicWeatherController %s: saw tornado warning — transition over %.1fs (OverallIntensity %.3f->%.3f, WindIntensity %.1f->%.1f, Fog %.2f->%.2f, WindVolume %.2f->%.2f)"),
		*GetName(), FadeDuration, SkyStartIntensity, TargetOverallIntensity, StartWindIntensity, TargetWindIntensity, FogStart, TargetFog, WindVolumeStart, TargetWindVolume);

	// Zero/negative duration: apply the targets immediately, no tick.
	if (FadeDuration <= 0.f)
	{
		ApplyAtAlpha(1.f);
		bFading = false;
		return;
	}

	SetActorTickEnabled(true);
}

void AUltraDynamicWeatherController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bFading)
	{
		return;
	}

	FadeElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(FadeElapsed / FadeDuration, 0.f, 1.f);
	ApplyAtAlpha(Alpha);

	if (Alpha >= 1.f)
	{
		bFading = false;
		SetActorTickEnabled(false);
		UE_LOG(LogTemp, Log, TEXT("AUltraDynamicWeatherController %s: storm transition complete (OverallIntensity %.3f, WindIntensity %.1f)"),
			*GetName(), TargetOverallIntensity, TargetWindIntensity);
	}
}

void AUltraDynamicWeatherController::ApplyAtAlpha(float Alpha)
{
	if (bFadeSky)
	{
		WriteFloat(SkyActor.Get(), GOverallIntensityProp, FMath::Lerp(SkyStartIntensity, TargetOverallIntensity, Alpha));
	}
	if (bRampWind)
	{
		WriteFloat(WeatherActor.Get(), GWindIntensityProp, FMath::Lerp(StartWindIntensity, TargetWindIntensity, Alpha));
		SetBool(WeatherActor.Get(), GWindUpdateNeededProp, true);
	}
	if (bRampFog)
	{
		WriteFloat(WeatherActor.Get(), GFogProp, FMath::Lerp(FogStart, TargetFog, Alpha));
		SetBool(WeatherActor.Get(), GFogUpdateNeededProp, true);
	}
	if (bLoudenWind)
	{
		if (UAudioComponent* AudioComp = AmbientGlobalWind ? AmbientGlobalWind->GetAudioComponent() : nullptr)
		{
			AudioComp->SetVolumeMultiplier(FMath::Lerp(WindVolumeStart, TargetWindVolume, Alpha));
		}
	}
}

AActor* AUltraDynamicWeatherController::FindActorByClassNeedle(const TCHAR* Needle) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetClass()->GetName().Contains(Needle))
		{
			return Actor;
		}
	}
	return nullptr;
}
