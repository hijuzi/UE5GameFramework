// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingScreenInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "LogLoadingScreenSystem.h"

bool ILevelLoadingScreenInterface::ShouldShowLevelLoadingScreen(UObject* TestObject, FString& OutReason)
{
	if (!TestObject)
	{
		return false;
	}

	if (const ILevelLoadingScreenInterface* Interface = Cast<ILevelLoadingScreenInterface>(TestObject))
	{
		return Interface->ShouldShowLevelLoadingScreen(OutReason);
	}

	return false;
}

FLevelLoadingScreenOverrideConfig ILevelLoadingScreenInterface::GetLevelLoadingScreenOverrideConfig(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return FLevelLoadingScreenOverrideConfig();
	}

	// 遍历所有 Actor，查找实现了接口的对象
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		ILevelLoadingScreenInterface* Interface = Cast<ILevelLoadingScreenInterface>(Actor);
		if (!Interface)
		{
			continue;
		}

		FLevelLoadingScreenOverrideConfig Config;
		Interface->GetLevelLoadingScreenOverrideConfig(Config);

		if (Config.bOverrideContent)
		{
			return Config;
		}
	}

	return FLevelLoadingScreenOverrideConfig();
}
