#include "WeirdplaceGameUserSettings.h"

UWeirdplaceGameUserSettings::UWeirdplaceGameUserSettings()
{
	GamepadLookSensitivity = DefaultGamepadLookSensitivity;
}

void UWeirdplaceGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	GamepadLookSensitivity = DefaultGamepadLookSensitivity;
}

float UWeirdplaceGameUserSettings::GetGamepadLookSensitivity() const
{
	return ClampAndSnap(GamepadLookSensitivity);
}

void UWeirdplaceGameUserSettings::SetGamepadLookSensitivity(float NewValue)
{
	GamepadLookSensitivity = ClampAndSnap(NewValue);
}

float UWeirdplaceGameUserSettings::ClampAndSnap(float Value)
{
	const float Clamped = FMath::Clamp(Value, MinGamepadLookSensitivity, MaxGamepadLookSensitivity);
	return FMath::RoundToFloat(Clamped / GamepadLookSensitivitySnap) * GamepadLookSensitivitySnap;
}
