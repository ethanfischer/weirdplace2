#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "PayPhone.generated.h"

class USoundBase;
class UAudioComponent;
class AMissingPersonPoster;
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

	// "Missing person" poster spawned on the pole as a SEPARATE actor (so the
	// SeenTornadoWarning hide on this scene's root doesn't take it down too).
	// Offset/yaw are relative to this actor — tune on the BP if placement drifts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Poster")
	FVector PosterRelativeOffset = FVector(25.f, 0.f, 150.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Poster")
	float PosterRelativeYaw = 0.f;

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

	UPROPERTY()
	AMissingPersonPoster* Poster = nullptr;

	bool bPlayedOnce = false;
	FDelegateHandle FlagChangedHandle;
};
