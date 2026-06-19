#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "PayPhone.generated.h"

class USoundBase;
class UAudioComponent;
enum class EStoryFlag : uint8;

// Roadside pay-phone scene. Hidden until the player has seen the tornado
// warning (SeenTornadoWarning); once revealed, pressing E once plays a bed of
// static + faint voices. BP_TelephoneScene is reparented onto this.
UCLASS()
class WEIRDPLACE2_API APayPhone : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APayPhone();

	// IInteractable
	virtual void Interact_Implementation() override;
	virtual bool CanInteract() override;

	// True while the static/voice bed is playing (test query).
	bool IsAudioPlaying() const;

	// Placeholder sounds (default-loaded if unset). Real static/voice swapped later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* StaticSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* VoiceSound = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnStoryFlagChanged(EStoryFlag Flag, bool bValue);
	void Reveal();

	UPROPERTY()
	UAudioComponent* StaticAudio = nullptr;

	UPROPERTY()
	UAudioComponent* VoiceAudio = nullptr;

	bool bPlayedOnce = false;
	FDelegateHandle FlagChangedHandle;
};
