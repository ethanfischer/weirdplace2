#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "UObject/WeakObjectPtr.h"

// Reusable character-by-character text reveal ("typewriter"), driven by the
// world timer manager. Owns the pacing and progress; the consumer renders via
// the callbacks (UMG TextBlock, TextRenderComponent, ...). Used by the NPC
// dialogue plates (UUI_Dialogue) and the payphone's diegetic code text.
//
// Lifetime: embed as a member of a UObject and pass that object as Owner to
// Start* — the timer callback is bound weakly to it, so the reveal goes dead
// with its owner. Callbacks fire only between Start* and completion/Stop.
class WEIRDPLACE2_API FTypewriterReveal
{
public:
	// Reveal one character every CharInterval seconds, the first after
	// FirstCharDelay. Restarts cleanly if already running.
	void Start(UObject* Owner, const FString& InFullText, float CharInterval, float FirstCharDelay);

	// Reveal paced so the LAST character lands after TotalDuration seconds —
	// for syncing the reveal to an audio clip of known length.
	void StartWithDuration(UObject* Owner, const FString& InFullText, float TotalDuration);

	// Cancel a reveal in progress (no OnFinished). Safe to call when idle.
	void Stop();

	bool IsFinished() const { return CharIndex >= FullText.Len(); }
	const FString& GetFullText() const { return FullText; }
	FString GetDisplayText() const { return FullText.Left(CharIndex); }

	// Called after each character with the text revealed so far.
	TFunction<void(const FString& DisplayText)> OnUpdate;

	// Called for each character as it is revealed (e.g. per-character blip).
	TFunction<void(TCHAR Character)> OnCharacterRevealed;

	// Called once when the last character has been revealed.
	TFunction<void()> OnFinished;

private:
	void RevealNext();

	TWeakObjectPtr<UObject> WeakOwner;
	FString FullText;
	int32 CharIndex = 0;
	float Interval = 0.03f;
	FTimerHandle TimerHandle;
};
