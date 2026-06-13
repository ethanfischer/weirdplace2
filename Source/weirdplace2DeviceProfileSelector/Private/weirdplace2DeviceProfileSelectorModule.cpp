#include "weirdplace2DeviceProfileSelectorModule.h"
#include "Misc/FileHelper.h"
#if PLATFORM_LINUX
#include "HAL/PlatformMisc.h"
#endif

IMPLEMENT_MODULE(FWeirdplace2DeviceProfileSelectorModule, weirdplace2DeviceProfileSelector);

void FWeirdplace2DeviceProfileSelectorModule::StartupModule()
{
#if PLATFORM_LINUX
	// UE 5.7's LinuxWindow.cpp calls SDL_StartTextInput on every window that
	// accepts input (per-window in SDL3). On Steam Deck that triggers the OS
	// keyboard. SDL3 hint SDL_ENABLE_SCREEN_KEYBOARD=0 disables auto-show, and
	// must be set BEFORE SDL_StartTextInput is called. This module loads at
	// PostConfigInit — earlier than window creation — so the env var is in
	// place when SDL queries the hint.
	FPlatformMisc::SetEnvironmentVar(TEXT("SDL_ENABLE_SCREEN_KEYBOARD"), TEXT("0"));
	UE_LOG(LogInit, Log, TEXT("Set SDL_ENABLE_SCREEN_KEYBOARD=0 to suppress Steam Deck on-screen keyboard"));
#endif
}

void FWeirdplace2DeviceProfileSelectorModule::ShutdownModule() {}

const FString FWeirdplace2DeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
#if PLATFORM_LINUX
	FString OsRelease;
	if (FFileHelper::LoadFileToString(OsRelease, TEXT("/etc/os-release")))
	{
		if (OsRelease.Contains(TEXT("ID=steamos")) || OsRelease.Contains(TEXT("ID=holo")))
		{
			UE_LOG(LogInit, Log, TEXT("Device Profile: Steam Deck detected"));
			return TEXT("SteamDeck");
		}
	}
#endif
	return FPlatformProperties::PlatformName();
}
