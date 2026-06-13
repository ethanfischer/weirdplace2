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

			"HeadMountedDisplay",

			"MediaAssets"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Editor-only dependencies for automation tests (FEndPlayMapCommand etc.)
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// Linux only: UE 5.7's LinuxWindow.cpp calls SDL_StartTextInput on every
		// window that accepts input. On Steam Deck, SDL3 text-input state is what
		// triggers the on-screen keyboard, so we stop it explicitly at game start.
		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL3");
		}

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
