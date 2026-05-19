using UnrealBuildTool;

public class weirdplace2DeviceProfileSelector : ModuleRules
{
	public weirdplace2DeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});
	}
}
