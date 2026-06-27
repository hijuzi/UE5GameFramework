// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PSOCacheSystemEditor : ModuleRules
{
	public PSOCacheSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"AssetRegistry",
			"AssetTools",
			"Niagara",
			"PSOCacheSystem",
		});
	}
}
