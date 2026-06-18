// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameModeBase.h"

#include "GameCoreGameMode.generated.h"

#define UE_API GAMECOREFRAMEWORK_API

/**
 * AGameCoreGameMode
 *
 * GameCore 框架默认 GameMode，预设核心类：
 * - HUDClass = AGameCoreHUD
 * - GameStateClass = ASVGameState
 * - PlayerControllerClass = AGameCorePlayerController
 *
 * 项目可继承此类以扩展自定义逻辑。
 */
UCLASS(MinimalAPI, config = Game)
class AGameCoreGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	UE_API AGameCoreGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

#undef UE_API
