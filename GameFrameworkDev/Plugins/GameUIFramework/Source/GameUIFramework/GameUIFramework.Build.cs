// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameUIFramework : ModuleRules
{
	public GameUIFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",
				"UMG",
				"CommonInput",
				"CommonUI",
				"GameplayTags",
				"GameplayAbilities",
				"ApplicationCore",
				"GameCoreFramework",
				"CommonUtility",
			}
		);
	}
}
