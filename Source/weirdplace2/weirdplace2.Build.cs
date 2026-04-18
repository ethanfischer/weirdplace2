// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class weirdplace2 : ModuleRules
{
	public weirdplace2(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"EnhancedInput",

			"HeadMountedDisplay"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Editor-only dependencies for BatchBlueprintToCodeCommandlet
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",        // For UCommandlet
				"AssetRegistry",   // For Blueprint discovery
				"NodeToCode",      // For translation API
				"BlueprintGraph",  // For K2Node types
				"Json",            // For JSON serialization
			});
		}

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
