#include "TestDriverToolset.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "ImageUtils.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"
#include "ToolsetRegistry/ToolCallAsyncResultVoid.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "Editor.h"
#include "FirstPersonCharacter.h"
#include "InputAction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MenuUIActor.h"
#include "MenuUIComponent.h"
#include "TestDriverSubsystem.h"

namespace
{
	void RaiseToolError(const FString& Message)
	{
		// The log line covers direct C++ callers (specs); RaiseScriptError only
		// fires when invoked through the script layer (MCP tool calls).
		UE_LOG(LogTemp, Error, TEXT("TestDriverToolset: %s"), *Message);
		UKismetSystemLibrary::RaiseScriptError(Message);
	}
}

UTestDriverSubsystem* UTestDriverToolset::GetDriverChecked()
{
	UWorld* PieWorld = GEditor ? GEditor->PlayWorld : nullptr;
	if (!PieWorld)
	{
		RaiseToolError(TEXT("No PIE session is running. Start PIE first (EditorAppToolset)."));
		return nullptr;
	}
	UTestDriverSubsystem* Driver = PieWorld->GetSubsystem<UTestDriverSubsystem>();
	if (!Driver || !Driver->IsPlayerReady())
	{
		RaiseToolError(TEXT("PIE is running but the player is not ready yet."));
		return nullptr;
	}
	return Driver;
}

UInputAction* UTestDriverToolset::ResolveActionChecked(const FString& ActionName)
{
	UTestDriverSubsystem* Driver = GetDriverChecked();
	if (!Driver)
	{
		return nullptr;
	}
	AFirstPersonCharacter* Player = Driver->GetPlayer();

	UInputAction* Action = nullptr;
	if      (ActionName == TEXT("Interact"))       Action = Player->GetInteractAction();
	else if (ActionName == TEXT("Inventory"))      Action = Player->GetInventoryAction();
	else if (ActionName == TEXT("Settings"))       Action = Player->GetSettingsAction();
	else if (ActionName == TEXT("NextOption"))     Action = Player->GetNextOptionAction();
	else if (ActionName == TEXT("PreviousOption")) Action = Player->GetPreviousOptionAction();
	else if (ActionName == TEXT("NavigateLeft"))   Action = Player->GetNavigateLeftAction();
	else if (ActionName == TEXT("NavigateRight"))  Action = Player->GetNavigateRightAction();
	else if (ActionName == TEXT("Back"))           Action = Player->GetBackAction();
	else
	{
		RaiseToolError(FString::Printf(TEXT("Unknown action '%s'. Valid: Interact, Inventory, Settings, NextOption, PreviousOption, NavigateLeft, NavigateRight, Back."), *ActionName));
		return nullptr;
	}

	if (!Action)
	{
		RaiseToolError(FString::Printf(TEXT("Action '%s' is not assigned on the player."), *ActionName));
	}
	return Action;
}

void UTestDriverToolset::PressInputAction(const FString& ActionName)
{
	UInputAction* Action = ResolveActionChecked(ActionName);
	if (!Action)
	{
		return;
	}
	UTestDriverSubsystem* Driver = GetDriverChecked();
	Driver->InjectInputAction(Action, true);

	// Release on the next frame so the Started trigger fires exactly once,
	// matching FTD_SimulateNavAction's press → 1-frame gap → release.
	TWeakObjectPtr<UTestDriverSubsystem> WeakDriver = Driver;
	TWeakObjectPtr<UInputAction> WeakAction = Action;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakDriver, WeakAction](float)
		{
			if (WeakDriver.IsValid() && WeakAction.IsValid())
			{
				WeakDriver->InjectInputAction(WeakAction.Get(), false);
			}
			return false; // one-shot
		}));
}

void UTestDriverToolset::SetInputActionPressed(const FString& ActionName, bool bPressed)
{
	UInputAction* Action = ResolveActionChecked(ActionName);
	if (!Action)
	{
		return;
	}
	GetDriverChecked()->InjectInputAction(Action, bPressed);
}

void UTestDriverToolset::PressKey(const FString& KeyName)
{
	UTestDriverSubsystem* Driver = GetDriverChecked();
	if (!Driver)
	{
		return;
	}
	const FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		RaiseToolError(FString::Printf(TEXT("'%s' is not a valid key name."), *KeyName));
		return;
	}
	Driver->SimulateKeyPress(Key);

	TWeakObjectPtr<UTestDriverSubsystem> WeakDriver = Driver;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakDriver, Key](float)
		{
			if (WeakDriver.IsValid())
			{
				WeakDriver->SimulateKeyRelease(Key);
			}
			return false;
		}));
}

namespace
{
	// The menu page name while the menu is fully open, empty otherwise.
	FString CurrentMenuPageName(UTestDriverSubsystem* Driver)
	{
		AFirstPersonCharacter* Player = Driver->GetPlayer();
		if (UMenuUIComponent* Menu = Player ? Player->GetMenuUIComponent() : nullptr)
		{
			if (Menu->IsFullyOpen())
			{
				if (AMenuUIActor* MenuActor = Menu->GetMenuActor())
				{
					return StaticEnum<EMenuPage>()->GetNameStringByValue(
						static_cast<int64>(MenuActor->GetCurrentPage()));
				}
			}
		}
		return FString();
	}

	// Polls Condition every frame until it returns true (SetCompleted) or
	// TimeoutSeconds elapse (SetError with TimeoutMessage()).
	UToolCallAsyncResultVoid* PollUntil(float TimeoutSeconds,
		TFunction<bool()> Condition, TFunction<FString()> TimeoutMessage)
	{
		UToolCallAsyncResultVoid* Result = NewObject<UToolCallAsyncResultVoid>();
		TStrongObjectPtr<UToolCallAsyncResultVoid> Pinned(Result);
		const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[Pinned, Deadline, Condition = MoveTemp(Condition), TimeoutMessage = MoveTemp(TimeoutMessage)](float) -> bool
			{
				if (Condition())
				{
					Pinned->SetCompleted();
					return false;
				}
				if (FPlatformTime::Seconds() > Deadline)
				{
					Pinned->SetError(TimeoutMessage());
					return false;
				}
				return true;
			}));
		return Result;
	}
}

UToolCallAsyncResultVoid* UTestDriverToolset::PressInputSequence(const TArray<FString>& ActionNames, float DelayBetween)
{
	UToolCallAsyncResultVoid* Result = NewObject<UToolCallAsyncResultVoid>();

	UTestDriverSubsystem* Driver = GetDriverChecked();
	if (!Driver)
	{
		Result->SetError(TEXT("No PIE session with a ready player."));
		return Result;
	}
	if (ActionNames.Num() == 0)
	{
		Result->SetError(TEXT("ActionNames is empty."));
		return Result;
	}

	// Ticker-driven state machine: press, release next frame, wait DelayBetween,
	// next action. Actions resolve at press time so a bad name fails mid-run
	// with a precise error.
	struct FSequenceState
	{
		TArray<FString> Actions;
		int32 Index = 0;
		bool bPressed = false;
		double NextPressTime = 0.0;
	};
	TSharedRef<FSequenceState> State = MakeShared<FSequenceState>();
	State->Actions = ActionNames;

	TStrongObjectPtr<UToolCallAsyncResultVoid> Pinned(Result);
	TWeakObjectPtr<UTestDriverSubsystem> WeakDriver(Driver);
	const float Delay = FMath::Max(DelayBetween, 0.f);

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Pinned, WeakDriver, State, Delay](float) -> bool
		{
			if (!WeakDriver.IsValid() || !WeakDriver->IsPlayerReady())
			{
				Pinned->SetError(FString::Printf(TEXT("PIE ended during the sequence (at action %d of %d)."),
					State->Index + 1, State->Actions.Num()));
				return false;
			}
			if (State->bPressed)
			{
				// Release the frame after the press, matching PressInputAction.
				if (UInputAction* Action = ResolveActionChecked(State->Actions[State->Index]))
				{
					WeakDriver->InjectInputAction(Action, false);
				}
				State->bPressed = false;
				State->Index++;
				if (State->Index >= State->Actions.Num())
				{
					Pinned->SetCompleted();
					return false;
				}
				State->NextPressTime = FPlatformTime::Seconds() + Delay;
				return true;
			}
			if (FPlatformTime::Seconds() < State->NextPressTime)
			{
				return true;
			}
			UInputAction* Action = ResolveActionChecked(State->Actions[State->Index]);
			if (!Action)
			{
				Pinned->SetError(FString::Printf(TEXT("Unknown or unassigned action '%s' (action %d of %d)."),
					*State->Actions[State->Index], State->Index + 1, State->Actions.Num()));
				return false;
			}
			WeakDriver->InjectInputAction(Action, true);
			State->bPressed = true;
			return true;
		}));

	return Result;
}

UToolCallAsyncResultVoid* UTestDriverToolset::WaitForActivityState(const FString& State, float TimeoutSeconds)
{
	UToolCallAsyncResultVoid* Early = nullptr;
	UTestDriverSubsystem* Driver = GetDriverChecked();
	if (!Driver)
	{
		Early = NewObject<UToolCallAsyncResultVoid>();
		Early->SetError(TEXT("No PIE session with a ready player."));
		return Early;
	}
	const UEnum* Enum = StaticEnum<EPlayerActivityState>();
	const int64 Wanted = Enum->GetValueByNameString(State);
	if (Wanted == INDEX_NONE)
	{
		Early = NewObject<UToolCallAsyncResultVoid>();
		Early->SetError(FString::Printf(TEXT("Unknown activity state '%s'. Valid: FreeRoaming, Interacting, InSimpleDialogue, InDialogue."), *State));
		return Early;
	}

	TWeakObjectPtr<UTestDriverSubsystem> WeakDriver(Driver);
	return PollUntil(TimeoutSeconds,
		[WeakDriver, Wanted]()
		{
			return WeakDriver.IsValid() && WeakDriver->IsPlayerReady()
				&& static_cast<int64>(WeakDriver->GetActivityState()) == Wanted;
		},
		[WeakDriver, State]()
		{
			const FString Current = WeakDriver.IsValid() && WeakDriver->IsPlayerReady()
				? StaticEnum<EPlayerActivityState>()->GetNameStringByValue(static_cast<int64>(WeakDriver->GetActivityState()))
				: FString(TEXT("<no player>"));
			return FString::Printf(TEXT("Timed out waiting for activity state '%s' (currently '%s')."), *State, *Current);
		});
}

UToolCallAsyncResultVoid* UTestDriverToolset::WaitForMenuPage(const FString& Page, float TimeoutSeconds)
{
	UTestDriverSubsystem* Driver = GetDriverChecked();
	if (!Driver)
	{
		UToolCallAsyncResultVoid* Early = NewObject<UToolCallAsyncResultVoid>();
		Early->SetError(TEXT("No PIE session with a ready player."));
		return Early;
	}
	if (StaticEnum<EMenuPage>()->GetValueByNameString(Page) == INDEX_NONE)
	{
		UToolCallAsyncResultVoid* Early = NewObject<UToolCallAsyncResultVoid>();
		Early->SetError(FString::Printf(TEXT("Unknown menu page '%s'. Valid: Pause, Settings, Graphics, Tunables."), *Page));
		return Early;
	}

	TWeakObjectPtr<UTestDriverSubsystem> WeakDriver(Driver);
	return PollUntil(TimeoutSeconds,
		[WeakDriver, Page]()
		{
			return WeakDriver.IsValid() && WeakDriver->IsPlayerReady()
				&& CurrentMenuPageName(WeakDriver.Get()) == Page;
		},
		[WeakDriver, Page]()
		{
			const FString Current = WeakDriver.IsValid() && WeakDriver->IsPlayerReady()
				? CurrentMenuPageName(WeakDriver.Get()) : FString(TEXT("<no player>"));
			return FString::Printf(TEXT("Timed out waiting for menu page '%s' (currently '%s')."),
				*Page, Current.IsEmpty() ? TEXT("<menu closed>") : *Current);
		});
}

FTestDriverPlayerStatus UTestDriverToolset::GetPlayerStatus()
{
	FTestDriverPlayerStatus Status;
	UTestDriverSubsystem* Driver = GetDriverChecked();
	if (!Driver)
	{
		return Status;
	}
	AFirstPersonCharacter* Player = Driver->GetPlayer();

	Status.ActivityState = StaticEnum<EPlayerActivityState>()->GetNameStringByValue(
		static_cast<int64>(Driver->GetActivityState()));
	Status.Location = Player->GetActorLocation();
	Status.MenuPage = CurrentMenuPageName(Driver);
	Status.bInventoryOpen = Driver->IsInventoryFullyOpen();
	return Status;
}

UToolCallAsyncResultString* UTestDriverToolset::CapturePlayerView(int32 MaxDimension)
{
	UToolCallAsyncResultString* Result = NewObject<UToolCallAsyncResultString>();
	MaxDimension = FMath::Clamp(MaxDimension, 64, 4096);

	UTestDriverSubsystem* Driver = GetDriverChecked();
	if (!Driver)
	{
		Result->SetError(TEXT("No PIE session with a ready player."));
		return Result;
	}
	UWorld* PieWorld = GEditor->PlayWorld;
	UGameViewportClient* Viewport = PieWorld ? PieWorld->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		Result->SetError(TEXT("PIE world has no game viewport."));
		return Result;
	}

	// Fixed path (overwritten each call) under Saved/Screenshots so the caller
	// can Read it without needing the name back.
	const FString OutPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), TEXT("PlayerView.png")));

	// Keep the result alive via the GC root rather than a TStrongObjectPtr in a
	// captured lambda: the screenshot delegate is stored on the viewport and
	// may be destroyed on a non-game thread, where TStrongObjectPtr asserts.
	// Captures below are all TWeakObjectPtr (safe to copy/destroy off-thread);
	// the UObject is only ever touched on the game thread.
	Result->AddToRoot();
	TWeakObjectPtr<UToolCallAsyncResultString> WeakResult(Result);
	TWeakObjectPtr<UGameViewportClient> WeakViewport(Viewport);
	TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();

	// Finishes the result exactly once, on the game thread, and unroots it.
	auto Complete = [WeakResult, WeakViewport, Handle](FString Path, FString ErrorMsg)
	{
		check(IsInGameThread());
		if (WeakViewport.IsValid())
		{
			WeakViewport->OnScreenshotCaptured().Remove(*Handle);
		}
		UToolCallAsyncResultString* R = WeakResult.Get();
		if (!R || R->bIsComplete)
		{
			return;
		}
		if (ErrorMsg.IsEmpty())
		{
			R->SetValue(Path);
		}
		else
		{
			R->SetError(ErrorMsg);
		}
		R->RemoveFromRoot();
	};

	*Handle = Viewport->OnScreenshotCaptured().AddLambda(
		[Complete, MaxDimension, OutPath](int32 Width, int32 Height, const TArray<FColor>& Colors)
		{
			// Downscale, PNG-encode, and write the file here (all thread-agnostic);
			// only the UObject touch in Complete must be on the game thread.
			FString ErrorMsg;

			const int32 LongEdge = FMath::Max(Width, Height);
			const TArray<FColor>* Pixels = &Colors;
			FIntPoint Dims(Width, Height);
			TArray<FColor> Resized;
			if (LongEdge > MaxDimension && Width > 0 && Height > 0)
			{
				const float Scale = static_cast<float>(MaxDimension) / LongEdge;
				const int32 DstW = FMath::Max(1, FMath::RoundToInt(Width * Scale));
				const int32 DstH = FMath::Max(1, FMath::RoundToInt(Height * Scale));
				Resized.SetNumUninitialized(DstW * DstH);
				FImageUtils::ImageResize(Width, Height, Colors, DstW, DstH, Resized,
					/*bResizeSRGBinLinearSpace*/ true);
				Pixels = &Resized;
				Dims = FIntPoint(DstW, DstH);
			}

			TArray64<uint8> Png;
			FImageView Image(Pixels->GetData(), Dims.X, Dims.Y, EGammaSpace::sRGB);
			if (!FImageUtils::CompressImage(Png, TEXT("png"), Image))
			{
				ErrorMsg = TEXT("Failed to PNG-encode the captured frame.");
			}
			else if (!FFileHelper::SaveArrayToFile(Png, *OutPath))
			{
				ErrorMsg = FString::Printf(TEXT("Failed to write screenshot to %s."), *OutPath);
			}

			if (IsInGameThread())
			{
				Complete(OutPath, MoveTemp(ErrorMsg));
			}
			else
			{
				AsyncTask(ENamedThreads::GameThread,
					[Complete, OutPath, ErrorMsg = MoveTemp(ErrorMsg)]() mutable
					{
						Complete(OutPath, MoveTemp(ErrorMsg));
					});
			}
		});

	// Failsafe (game-thread ticker): if no frame arrives, error out instead of
	// hanging the tool call.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakResult, Complete](float) -> bool
		{
			UToolCallAsyncResultString* R = WeakResult.Get();
			if (!R || R->bIsComplete)
			{
				return false;
			}
			Complete(FString(), TEXT("Timed out waiting for the screenshot capture."));
			return false;
		}), 10.f);

	// bShowUI=false: capture the rendered scene only, not editor slate chrome.
	// The game's diegetic menu is in-world geometry, so it still appears.
	FScreenshotRequest::RequestScreenshot(/*bInShowUI*/ false);
	return Result;
}

void UTestDriverToolset::TeleportToWaypoint(const FString& WaypointTag)
{
	UTestDriverSubsystem* Driver = GetDriverChecked();
	if (!Driver)
	{
		return;
	}
	if (!Driver->TeleportPlayerToWaypoint(FName(*WaypointTag)))
	{
		RaiseToolError(FString::Printf(TEXT("No ATestWaypoint with tag '%s'."), *WaypointTag));
	}
}

void UTestDriverToolset::TeleportNearActor(const FString& ActorLabel, float Distance)
{
	UTestDriverSubsystem* Driver = GetDriverChecked();
	if (!Driver)
	{
		return;
	}
	AActor* Target = Driver->FindActorByLabel(ActorLabel);
	if (!Target)
	{
		RaiseToolError(FString::Printf(TEXT("No actor with label '%s'."), *ActorLabel));
		return;
	}
	Driver->TeleportNearActor(Target, Distance);
}

void UTestDriverToolset::LookAtActor(const FString& ActorLabel)
{
	UTestDriverSubsystem* Driver = GetDriverChecked();
	if (!Driver)
	{
		return;
	}
	if (!Driver->LookAtActorByLabel(ActorLabel))
	{
		RaiseToolError(FString::Printf(TEXT("No actor with label '%s'."), *ActorLabel));
	}
}
