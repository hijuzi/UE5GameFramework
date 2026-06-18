// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerController.h"

#include "GameCorePlayerController.generated.h"

#define UE_API GAMECOREFRAMEWORK_API

/**
 * AGameCorePlayerController
 * 
 * 在 ReceivedPlayer 时广播 UGameCoreLocalPlayer::OnPlayerControllerSet，
 * 通知 UIPolicy PC 已可用，以便创建 UI Layout。
 * 参考 Lyra ACommonPlayerController。
 */
UCLASS(MinimalAPI, config = Game)
class AGameCorePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UE_API AGameCorePlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin APlayerController Interface
	UE_API virtual void ReceivedPlayer() override;
	//~ End APlayerController Interface
};

#undef UE_API
