// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameState.h"

#include "SVGameState.generated.h"

#define UE_API SVRUNTIME_API

class USVExperienceManagerComponent;

/**
 * ASVGameState
 *
 * SV 框架核心 GameState，作为所有项目 GameState 的基类。
 * 提供 Experience 加载管理等底层能力。
 */
UCLASS(MinimalAPI)
class ASVGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ASVGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin AActor Interface
	virtual void PostInitializeComponents() override;
	//~ End AActor Interface

	/** Experience 管理组件（GameState 自有子对象） */
	UPROPERTY()
	TObjectPtr<USVExperienceManagerComponent> ExperienceManagerComponent;

protected:
	/** Experience 管理组件类（可在子类或蓝图中配置） */
	UPROPERTY(EditDefaultsOnly, Category = "Experience")
	TSubclassOf<USVExperienceManagerComponent> ExperienceManagerComponentClass;
};

#undef UE_API
