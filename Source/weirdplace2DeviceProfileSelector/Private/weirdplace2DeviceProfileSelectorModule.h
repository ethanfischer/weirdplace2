#pragma once

#include "Modules/ModuleManager.h"
#include "IDeviceProfileSelectorModule.h"

class FWeirdplace2DeviceProfileSelectorModule : public IDeviceProfileSelectorModule
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual const FString GetRuntimeDeviceProfileName() override;
};
