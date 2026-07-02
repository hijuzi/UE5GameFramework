// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SVRuntime : ModuleRules
{
	public SVRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"UMG",
				"CommonInput",
				"CommonUI",
				"GameplayTags",
				"GameplayAbilities",
				"ApplicationCore",
				"GameCoreFramework",
				"GameUIFramework",
				"CommonUtility",
				"LoadingScreenSystem",
				"ControlFlows",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PSOCacheSystem",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
