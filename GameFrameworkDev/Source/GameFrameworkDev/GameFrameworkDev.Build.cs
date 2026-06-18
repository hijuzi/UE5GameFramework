// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameFrameworkDev : ModuleRules
{
	public GameFrameworkDev(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"GameCoreFramework",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"GameFrameworkDev",
			"GameFrameworkDev/Variant_Platforming",
			"GameFrameworkDev/Variant_Platforming/Animation",
			"GameFrameworkDev/Variant_Combat",
			"GameFrameworkDev/Variant_Combat/AI",
			"GameFrameworkDev/Variant_Combat/Animation",
			"GameFrameworkDev/Variant_Combat/Gameplay",
			"GameFrameworkDev/Variant_Combat/Interfaces",
			"GameFrameworkDev/Variant_Combat/UI",
			"GameFrameworkDev/Variant_SideScrolling",
			"GameFrameworkDev/Variant_SideScrolling/AI",
			"GameFrameworkDev/Variant_SideScrolling/Gameplay",
			"GameFrameworkDev/Variant_SideScrolling/Interfaces",
			"GameFrameworkDev/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
