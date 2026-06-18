// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/HUD.h"

#include "GameCoreHUD.generated.h"

namespace EEndPlayReason { enum Type : int; }

class AActor;
class UObject;

/**
 * AGameCoreHUD
 *
 *  注意：通常不需要扩展或修改此类，而是在 Experience 中使用 "Add Widget" 操作
 *  来添加 HUD 布局及其中的控件。
 * 
 *  此类主要用于调试渲染。
 */
UCLASS(Config = Game)
class AGameCoreHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGameCoreHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

	//~AActor 接口
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor 接口

	//~AHUD 接口
	virtual void GetDebugActorList(TArray<AActor*>& InOutList) override;
	//~End of AHUD 接口
};
