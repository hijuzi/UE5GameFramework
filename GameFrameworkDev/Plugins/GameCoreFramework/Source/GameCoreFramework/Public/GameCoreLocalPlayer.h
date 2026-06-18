// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/LocalPlayer.h"

#include "GameCoreLocalPlayer.generated.h"

class APlayerController;

/**
 * UGameCoreLocalPlayer
 * 
 * 扩展 ULocalPlayer，提供 OnPlayerControllerSet 委托，
 * 用于在 PlayerController 可用时通知 UIPolicy 创建 Layout。
 * 参考 Lyra UCommonLocalPlayer。
 */
UCLASS(config = Engine, transient)
class GAMECOREFRAMEWORK_API UGameCoreLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	UGameCoreLocalPlayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 当此 LocalPlayer 被分配 PlayerController 时广播 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FPlayerControllerSetDelegate, UGameCoreLocalPlayer* LocalPlayer, APlayerController* PlayerController);
	FPlayerControllerSetDelegate OnPlayerControllerSet;
};
