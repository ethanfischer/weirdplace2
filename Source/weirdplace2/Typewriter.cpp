#include "Typewriter.h"

#include "Engine/World.h"
#include "TimerManager.h"

void FTypewriterReveal::Start(UObject* Owner, const FString& InFullText, float CharInterval, float FirstCharDelay)
{
	Stop();

	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("FTypewriterReveal::Start: no owner/world — reveal not started"));
		return;
	}

	WeakOwner = Owner;
	FullText = InFullText;
	CharIndex = 0;
	Interval = FMath::Max(0.001f, CharInterval);

	FTimerDelegate Delegate = FTimerDelegate::CreateWeakLambda(Owner, [this]()
	{
		RevealNext();
	});
	World->GetTimerManager().SetTimer(TimerHandle, Delegate, FMath::Max(0.001f, FirstCharDelay), false);
}

void FTypewriterReveal::StartWithDuration(UObject* Owner, const FString& InFullText, float TotalDuration)
{
	const int32 Len = InFullText.Len();
	const float PerChar = Len > 0 ? TotalDuration / Len : 0.03f;
	Start(Owner, InFullText, PerChar, PerChar);
}

void FTypewriterReveal::Stop()
{
	if (UObject* Owner = WeakOwner.Get())
	{
		if (UWorld* World = Owner->GetWorld())
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
	}
	WeakOwner.Reset();
}

void FTypewriterReveal::RevealNext()
{
	UObject* Owner = WeakOwner.Get();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || CharIndex >= FullText.Len())
	{
		return;
	}

	const TCHAR NewChar = FullText[CharIndex];
	CharIndex++;

	if (OnUpdate)
	{
		OnUpdate(FullText.Left(CharIndex));
	}
	if (OnCharacterRevealed)
	{
		OnCharacterRevealed(NewChar);
	}

	if (CharIndex >= FullText.Len())
	{
		if (OnFinished)
		{
			OnFinished();
		}
		return;
	}

	FTimerDelegate Delegate = FTimerDelegate::CreateWeakLambda(Owner, [this]()
	{
		RevealNext();
	});
	World->GetTimerManager().SetTimer(TimerHandle, Delegate, Interval, false);
}
