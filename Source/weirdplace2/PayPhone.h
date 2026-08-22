#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "Interactable.h"
#include "Typewriter.h"
#include "PayPhone.generated.h"

class USoundBase;
class UAudioComponent;
class UCableComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
enum class EStoryFlag : uint8;

// Roadside pay-phone scene. Hidden until the player has seen the tornado
// warning (SeenTornadoWarning); once revealed, interacting picks up the
// receiver: the handset mesh animates from the cradle to the player's ear,
// and on the FIRST call the static + faint voices come up immediately, the
// spoken bathroom code follows after a short beat, then a busy tone. The
// player cannot hang up until the code has fully played. Later calls are
// mundane — just the looping dialtone — and can be hung up freely via
// "Exit Interaction" (Q / gamepad B). BP_TelephoneScene is reparented onto
// this; the kiosk mesh is swapped at BeginPlay for the handset-less body so
// the receiver can move independently.
UCLASS()
class WEIRDPLACE2_API APayPhone : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APayPhone();

	virtual void Tick(float DeltaSeconds) override;

	// IInteractable
	virtual void Interact_Implementation() override;
	virtual bool CanInteract() override;

	// Only the payphone kiosk answers — the telephone pole (same actor) doesn't.
	virtual bool IsComponentInteractable(const UPrimitiveComponent* Component) override;

	// Hang up the receiver: stop the audio, play the hangup one-shot, animate
	// the handset back to the cradle, and release the player. Bound to "Exit
	// Interaction" while off the hook; also called directly by the E2E
	// TestDriver. Ignored while the first-call code is still playing.
	void HangUp();

	// True while any of the pickup/dialtone/static/voice components is playing
	// (test query).
	bool IsAudioPlaying() const;

	// True while the dialtone loop is playing (test query).
	bool IsDialtonePlaying() const;

	// True while hang-up is refused because the first-call code hasn't finished
	// (test query).
	bool IsHangupLocked() const { return bHangupLocked; }

	// Test seam: mark the spoken code as already heard, so the next pickup is a
	// mundane "dialtone only" call (the persistent looping dialtone, no code).
	void MarkCodeSpokenForTest() { bCodeSpoken = true; }

	// Placeholder sounds (default-loaded if unset). Real static/voice swapped later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* StaticSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* VoiceSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* PickupSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* DialtoneSound = nullptr;

	// Spoken bathroom code — plays ONCE for the whole game, on the first call.
	// The player is held on the line until it finishes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* CodeSound = nullptr;

	// Busy-signal tone, played a few seconds after the spoken code finishes (first
	// call only). Supplied later — stays silent until then.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* BusySound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	USoundBase* HangupSound = nullptr;

	// Seconds after the static/voices begin before the spoken code plays (first call).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	float CodeSpeechDelay = 1.5f;

	// Seconds after the spoken code finishes before the busy tone plays.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone")
	float BusyToneDelay = 3.0f;

	// Handset-less kiosk body + standalone handset (default-loaded if unset).
	// Split from SM_Payphone_NN_01a; the receiver pivots at its own center.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Receiver")
	UStaticMesh* BodyMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Receiver")
	UStaticMesh* HandsetMesh = nullptr;

	// Where the handset sits in the cradle, relative to the kiosk mesh
	// (matches the split-out geometry's center in the original mesh).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Receiver")
	FVector ReceiverCradleOffset = FVector(0.38f, 12.45f, 111.37f);

	// Held pose relative to the player camera (X fwd, Y right, Z up). Sits just
	// BEHIND the camera on the left — the lift animation sweeps through view,
	// then the handset parks out of sight so it never occludes the scene.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Receiver")
	FVector ReceiverEarOffset = FVector(-12.0f, -16.0f, -6.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Receiver")
	FRotator ReceiverEarRotation = FRotator(-25.0f, 160.0f, -20.0f);

	// Seconds for the cradle <-> ear handset animation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Receiver")
	float ReceiverAnimDuration = 0.45f;

	// Where the cord roots on the kiosk (relative to the kiosk mesh) — where
	// the original curly cord anchored on the phone unit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Cord")
	FVector CordAnchorOffset = FVector(-9.0f, 11.0f, 70.0f);

	// Slack length of the cable cord (cm). Short enough that the on-hook drape
	// stays above the booth shelf; while held it pulls taut regardless.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Cord")
	float CordLength = 38.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PayPhone|Cord")
	float CordWidth = 1.2f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnStoryFlagChanged(EStoryFlag Flag, bool bValue);
	void Reveal();

	// Swap the authored kiosk mesh (handset baked in) for the handset-less body
	// and spawn the standalone receiver mesh in the cradle.
	void SetUpReceiver();

	// Begin interpolating the receiver's relative transform (against its current
	// parent) to the given target pose.
	void StartReceiverAnim(const FVector& TargetLocation, const FRotator& TargetRotation);

	// Timer callback: the pickup one-shot has finished — start the call audio.
	// First call: static + voices immediately, code after CodeSpeechDelay.
	// Later calls: just the looping dialtone.
	void StartCall();

	// Timer callback (CodeSpeechDelay after the call audio starts): play the
	// spoken code. First call only.
	void PlayCodeOnce();

	// Timer callback (code duration after PlayCodeOnce): the code has fully
	// played — latch bCodeSpoken, unlock hang-up, arm the busy tone.
	void OnCodeFinished();

	// Timer callback (BusyToneDelay after the code finishes): play the busy tone.
	void PlayBusyTone();

	// Stop the typewriter and hide the diegetic text (hang-up / teardown).
	void ResetCodeText();

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
	UAudioComponent* CodeAudio = nullptr;

	UPROPERTY()
	UAudioComponent* BusyAudio = nullptr;

	UPROPERTY()
	UAudioComponent* HangupAudio = nullptr;

	// The kiosk mesh component (authored in the BP; mesh swapped to BodyMesh).
	UPROPERTY()
	UStaticMeshComponent* KioskMesh = nullptr;

	// The standalone handset, spawned at BeginPlay. Reparented between the
	// kiosk (cradled) and the player camera (held).
	UPROPERTY()
	UStaticMeshComponent* ReceiverMesh = nullptr;

	// Simulated cord from the kiosk to the receiver — the only cord (the baked
	// curly cord was removed from the body mesh). Drapes to the cradle on-hook,
	// stretches toward the player while held.
	UPROPERTY()
	UCableComponent* CordCable = nullptr;

	// The "DiegeticText" TextRender authored in BP_TelephoneScene. Its authored
	// text is the full line; hidden until the spoken code plays, then revealed
	// typewriter-style in sync with the audio. Named differently from the BP
	// component — sharing the name breaks the BP compile (property collision).
	UPROPERTY()
	UTextRenderComponent* CodeTextRender = nullptr;

	// Full authored DiegeticText line, cached at BeginPlay before blanking.
	FString CodeFullText;

	// Reveals CodeFullText onto DiegeticText, paced to the code audio's length.
	FTypewriterReveal CodeTypewriter;

	// True while the receiver is up (between pickup and hang up).
	bool bOffHook = false;

	// The spoken code plays once for the whole game; latched true once it has
	// FULLY played (not merely started).
	bool bCodeSpoken = false;

	// True from first-call pickup until the code finishes — HangUp is refused.
	bool bHangupLocked = false;

	// Receiver animation state (relative to the receiver's current parent).
	bool bReceiverAnimating = false;
	float ReceiverAnimElapsed = 0.0f;
	FVector ReceiverAnimStartLoc = FVector::ZeroVector;
	FRotator ReceiverAnimStartRot = FRotator::ZeroRotator;
	FVector ReceiverAnimTargetLoc = FVector::ZeroVector;
	FRotator ReceiverAnimTargetRot = FRotator::ZeroRotator;

	FDelegateHandle FlagChangedHandle;
	FTimerHandle CallStartTimer;
	FTimerHandle CodeSpeechTimer;
	FTimerHandle CodeEndTimer;
	FTimerHandle BusyToneTimer;
};
