// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/LocalPlayer.h"

#include "GameCoreLocalPlayer.generated.h"

class APlayerController;
class APlayerState;
class APawn;

/**
 * UGameCoreLocalPlayer
 * 
 * 扩展 ULocalPlayer，提供 OnPlayerControllerSet/OnPlayerStateSet/OnPlayerPawnSet 委托，
 * 用于在 PlayerController/PlayerState/Pawn 可用时通知其他系统。
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

	/** 当此 LocalPlayer 对应 PlayerController 获得 PlayerState 时广播（移植自 Lyra CommonLocalPlayer） */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FPlayerStateSetDelegate, UGameCoreLocalPlayer* LocalPlayer, APlayerState* PlayerState);
	FPlayerStateSetDelegate OnPlayerStateSet;
	
	/** 当此 LocalPlayer 对应 PlayerController 获得 Pawn 时广播 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FPlayerPawnSetDelegate, UGameCoreLocalPlayer* LocalPlayer, APawn* Pawn);
	FPlayerPawnSetDelegate OnPlayerPawnSet;

	/** 注册 PlayerController 回调，若已就绪则立即调用 */
	FDelegateHandle CallAndRegister_OnPlayerControllerSet(FPlayerControllerSetDelegate::FDelegate Delegate);
	/** 注册 PlayerState 回调，若已就绪则立即调用 */
	FDelegateHandle CallAndRegister_OnPlayerStateSet(FPlayerStateSetDelegate::FDelegate Delegate);
	FDelegateHandle CallAndRegister_OnPlayerPawnSet(FPlayerPawnSetDelegate::FDelegate Delegate);
};
