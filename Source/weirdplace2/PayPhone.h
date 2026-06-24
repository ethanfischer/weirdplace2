#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "Interactable.h"
#include "PayPhone.generated.h"

class USoundBase;
class UAudioComponent;
enum class EStoryFlag : uint8;

// Roadside pay-phone scene. Hidden until the player has seen the tornado
// warning (SeenTornadoWarning); once revealed, interacting picks up the
// receiver: a one-shot pickup, then a looping dialtone (with the existing
// static + faint voices bleeding over it). The player is held at the phone
// until they press "Exit Interaction" (Q / gamepad B), which stops the
// dialtone, plays a hangup one-shot, and releases them. Repeatable.
// BP_TelephoneScene is reparented onto this.
UCLASS()
class WEIRDPLACE2_API APayPhone : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APayPhone();

	// IInteractable
	virtual void Interact_Implementation() override;
	virtual bool CanInteract() override;

	// Hang up the receiver: stop the dialtone/static/voices, play the hangup
	// one-shot, and release the player. Bound to "Exit Interaction" while off
	// the hook; also called directly by the E2E TestDriver.
	void HangUp();

	// True while any of the pickup/dialtone/static/voice components is playing
	// (test query).
	bool IsAudioPlaying() const;

	// True while the dialtone loop is playing (test query).
	bool IsDialtonePlaying() const;

	// Placeholder sounds (default-loaded if unset). Real static/voice swapped later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* StaticSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* VoiceSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* PickupSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* DialtoneSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* HangupSound = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnStoryFlagChanged(EStoryFlag Flag, bool bValue);
	void Reveal();

	// Timer callback: start the dialtone loop + static/voices once the pickup
	// one-shot has finished playing.
	void StartDialtone();

	// Release the player's movement and remove the "Exit Interaction" binding.
	// Shared by HangUp and EndPlay (mid-call teardown).
	void ReleasePlayer();

	UPROPERTY()
	UAudioComponent* StaticAudio = nullptr;

	UPROPERTY()
	UAudioComponent* VoiceAudio = nullptr;

	UPROPERTY()
	UAudioComponent* PickupAudio = nullptr;

	UPROPERTY()
	UAudioComponent* DialtoneAudio = nullptr;

	UPROPERTY()
	UAudioComponent* HangupAudio = nullptr;

	// True while the receiver is up (between pickup and hang up).
	bool bOffHook = false;

	FDelegateHandle FlagChangedHandle;
	FTimerHandle DialtoneStartTimer;
};
