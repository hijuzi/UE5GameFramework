// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LoadingScreenSystem : ModuleRules
{
	public LoadingScreenSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UMG",
				"DeveloperSettings",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"MoviePlayer",
				"InputCore",
				"RenderCore",
				"PreLoadScreen",
			}
			);
	}
}
