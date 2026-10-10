using UnrealBuildTool;

// Editor-only module: AI toolsets (unreal-mcp) and other tooling that must not
// ship in game builds.
public class weirdplace2Editor : ModuleRules
{
	public weirdplace2Editor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"weirdplace2"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EnhancedInput",
			"ImageWrapper",
			"InputCore",
			"ToolsetRegistry",
			"UnrealEd"
		});
	}
}
