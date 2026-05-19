#include "weirdplace2DeviceProfileSelectorModule.h"
#include "Misc/FileHelper.h"

IMPLEMENT_MODULE(FWeirdplace2DeviceProfileSelectorModule, weirdplace2DeviceProfileSelector);

void FWeirdplace2DeviceProfileSelectorModule::StartupModule() {}
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
