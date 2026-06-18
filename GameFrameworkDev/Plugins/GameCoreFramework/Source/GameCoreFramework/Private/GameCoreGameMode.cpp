// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameCoreGameMode.h"
#include "GameCoreHUD.h"
#include "GameCorePlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameCoreGameMode)

AGameCoreGameMode::AGameCoreGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HUDClass = AGameCoreHUD::StaticClass();
	PlayerControllerClass = AGameCorePlayerController::StaticClass();
	// GameStateClass 由子类或蓝图设置（如 ASVGameState），
	// 避免 GameCoreFramework 直接依赖 SVRuntime 造成循环引用。
}
