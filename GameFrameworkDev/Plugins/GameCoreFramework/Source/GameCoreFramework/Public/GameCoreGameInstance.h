// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"

#include "GameCoreGameInstance.generated.h"

#define UE_API GAMECOREFRAMEWORK_API

class ULocalPlayer;

/**
 * UGameCoreGameInstance
 * 
 * 管理 LocalPlayer 的添加与移除，通过委托通知系统。
 * 参考 Lyra UCommonGameInstance。
 */
UCLASS(MinimalAPI, Abstract, config = Game)
class UGameCoreGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UGameCoreGameInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 本地玩家添加时广播 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameCorePlayerAdded, ULocalPlayer* /*NewPlayer*/);
	FOnGameCorePlayerAdded OnPlayerAdded;

	/** 本地玩家销毁时广播 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameCorePlayerDestroyed, ULocalPlayer* /*ExistingPlayer*/);
	FOnGameCorePlayerDestroyed OnPlayerDestroyed;

	/** 返回当前的主玩家（本地玩家中被标记为 Primary 的那个） */
	UE_API ULocalPlayer* GetPrimaryPlayer() const;

	//~ Begin UGameInstance Interface
	UE_API virtual int32   AddLocalPlayer(ULocalPlayer* NewPlayer, FPlatformUserId UserId) override;
	UE_API virtual bool    RemoveLocalPlayer(ULocalPlayer* ExistingPlayer) override;
	//~ End UGameInstance Interface

private:
	/** 第一个添加的本地玩家 */
	TWeakObjectPtr<ULocalPlayer> PrimaryPlayer;
};

#undef UE_API
