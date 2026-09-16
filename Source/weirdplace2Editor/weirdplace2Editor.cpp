#include "Modules/ModuleManager.h"
#include "TestDriverToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

class FWeirdplace2EditorModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UTestDriverToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		UToolsetRegistry::UnregisterToolsetClass(UTestDriverToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FWeirdplace2EditorModule, weirdplace2Editor);
