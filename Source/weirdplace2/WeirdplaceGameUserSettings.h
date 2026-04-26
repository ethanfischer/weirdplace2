#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "WeirdplaceGameUserSettings.generated.h"

UCLASS(Config=GameUserSettings)
class WEIRDPLACE2_API UWeirdplaceGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UWeirdplaceGameUserSettings();

	virtual void SetToDefaults() override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weirdplace Settings")
	float GetGamepadLookSensitivity() const;

	UFUNCTION(BlueprintCallable, Category = "Weirdplace Settings")
	void SetGamepadLookSensitivity(float NewValue);

	static constexpr float MinGamepadLookSensitivity = 0.1f;
	static constexpr float MaxGamepadLookSensitivity = 2.0f;
	static constexpr float GamepadLookSensitivitySnap = 0.1f;
	static constexpr float DefaultGamepadLookSensitivity = 1.0f;

	// User-facing sensitivity is a "feel" number; multiply by this to get the actual
	// scale applied to AddControllerYawInput/PitchInput. Tuned so user-facing 1.0
	// matches a comfortable baseline.
	static constexpr float GamepadLookSensitivityScaleFactor = 0.5f;

private:
	UPROPERTY(Config)
	float GamepadLookSensitivity;

	static float ClampAndSnap(float Value);
};
