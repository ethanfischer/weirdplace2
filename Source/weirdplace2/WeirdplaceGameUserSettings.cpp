#include "WeirdplaceGameUserSettings.h"

UWeirdplaceGameUserSettings::UWeirdplaceGameUserSettings()
{
	GamepadLookSensitivity = DefaultGamepadLookSensitivity;
	MouseLookSensitivity = DefaultMouseLookSensitivity;
}

void UWeirdplaceGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	GamepadLookSensitivity = DefaultGamepadLookSensitivity;
	MouseLookSensitivity = DefaultMouseLookSensitivity;
}

float UWeirdplaceGameUserSettings::GetGamepadLookSensitivity() const
{
	return ClampAndSnap(GamepadLookSensitivity, MinGamepadLookSensitivity, MaxGamepadLookSensitivity, GamepadLookSensitivitySnap);
}

void UWeirdplaceGameUserSettings::SetGamepadLookSensitivity(float NewValue)
{
	GamepadLookSensitivity = ClampAndSnap(NewValue, MinGamepadLookSensitivity, MaxGamepadLookSensitivity, GamepadLookSensitivitySnap);
}

float UWeirdplaceGameUserSettings::GetMouseLookSensitivity() const
{
	return ClampAndSnap(MouseLookSensitivity, MinMouseLookSensitivity, MaxMouseLookSensitivity, MouseLookSensitivitySnap);
}

void UWeirdplaceGameUserSettings::SetMouseLookSensitivity(float NewValue)
{
	MouseLookSensitivity = ClampAndSnap(NewValue, MinMouseLookSensitivity, MaxMouseLookSensitivity, MouseLookSensitivitySnap);
}

float UWeirdplaceGameUserSettings::ClampAndSnap(float Value, float Min, float Max, float Snap)
{
	const float Clamped = FMath::Clamp(Value, Min, Max);
	return FMath::RoundToFloat(Clamped / Snap) * Snap;
}
