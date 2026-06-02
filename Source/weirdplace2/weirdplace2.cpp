// Fill out your copyright notice in the Description page of Project Settings.

#include "weirdplace2.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_LINUX
#include "Misc/CoreDelegates.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "Widgets/SWindow.h"
THIRD_PARTY_INCLUDES_START
#include <SDL3/SDL.h>
THIRD_PARTY_INCLUDES_END

// UE 5.7's LinuxWindow.cpp calls SDL_StartTextInput on every window that
// accepts input (SDL3 made text-input per-window). Steam Deck's overlay
// watches SDL text-input state and shows the OS keyboard while it's active.
// Stop it as early as we can — right after engine init completes, before any
// gameplay world loads. This runs before AFirstPersonCharacter::BeginPlay.
class FWeirdplaceLinuxTextInputSuppressor
{
public:
	void Register()
	{
		FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FWeirdplaceLinuxTextInputSuppressor::Run);
	}

	void Unregister()
	{
		FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	}

private:
	void Run()
	{
		if (!FSlateApplication::IsInitialized())
		{
			UE_LOG(LogTemp, Warning, TEXT("Linux text-input suppressor: Slate not initialized"));
			return;
		}

		TArray<TSharedRef<SWindow>> AllWindows = FSlateApplication::Get().GetInteractiveTopLevelWindows();
		UE_LOG(LogTemp, Display, TEXT("Linux text-input suppressor: OnFEngineLoopInitComplete, %d top-level windows"), AllWindows.Num());
		for (int32 i = 0; i < AllWindows.Num(); ++i)
		{
			TSharedPtr<FGenericWindow> NativeWindow = AllWindows[i]->GetNativeWindow();
			if (!NativeWindow.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("Linux text-input suppressor[%d]: native window null"), i);
				continue;
			}
			SDL_Window* SDLWindow = static_cast<SDL_Window*>(NativeWindow->GetOSWindowHandle());
			if (!SDLWindow)
			{
				UE_LOG(LogTemp, Warning, TEXT("Linux text-input suppressor[%d]: SDL handle null"), i);
				continue;
			}
			const bool bWasActive = SDL_TextInputActive(SDLWindow);
			const bool bStopped = SDL_StopTextInput(SDLWindow);
			UE_LOG(LogTemp, Display, TEXT("Linux text-input suppressor[%d]: handle=%p wasActive=%d ok=%d"), i, SDLWindow, bWasActive, bStopped);
		}
	}
};

static FWeirdplaceLinuxTextInputSuppressor GLinuxTextInputSuppressor;

class FWeirdplace2GameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		GLinuxTextInputSuppressor.Register();
	}

	virtual void ShutdownModule() override
	{
		GLinuxTextInputSuppressor.Unregister();
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FWeirdplace2GameModule, weirdplace2, "weirdplace2");
#else
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, weirdplace2, "weirdplace2");
#endif
