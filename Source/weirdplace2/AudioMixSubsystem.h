#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "AudioMixSubsystem.generated.h"

class USoundMix;

// The game's mixer. One weird.Audio.* tunable per SoundClass in
// Content/Sounds/Classes (see AudioMixSubsystem.cpp); this subsystem pushes them
// onto the audio device as sound-mix class overrides so they apply live, in-game,
// from the Tunables menu. Bake dialed-in values into the WP_TUNABLE defaults.
UCLASS()
class UAudioMixSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	// Re-push every class volume from its cvar.
	void Apply();

	UPROPERTY()
	TObjectPtr<USoundMix> Mix;

	FConsoleVariableSinkHandle SinkHandle;
};
