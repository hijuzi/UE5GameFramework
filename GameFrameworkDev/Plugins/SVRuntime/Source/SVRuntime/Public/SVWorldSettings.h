// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/WorldSettings.h"

#include "SVWorldSettings.generated.h"

#define UE_API SVRUNTIME_API

class USVBaseExperienceDefinition;

/**
 * ASVWorldSettings
 *
 * 默认的世界设置，主要用于配置该地图的默认 Gameplay Experience。
 * 当服务器打开此地图且未被用户覆盖时，将使用此 Experience。
 *
 * 参考 Lyra ALyraWorldSettings。
 */
UCLASS(MinimalAPI)
class ASVWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	ASVWorldSettings(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	UE_API virtual void CheckForErrors() override;
#endif

	/** 获取默认的 Gameplay Experience 资产 ID */
	UE_API FPrimaryAssetId GetDefaultGameplayExperience() const;

	/** 获取默认的 Gameplay Experience 软引用 */
	const TSoftClassPtr<USVBaseExperienceDefinition>& GetDefaultGameplayExperienceSoftPtr() const { return DefaultGameplayExperience; }

protected:
	/** 该地图的默认 Experience，若未被用户覆盖则使用此配置 */
	UPROPERTY(EditDefaultsOnly, Category = GameMode)
	TSoftClassPtr<USVBaseExperienceDefinition> DefaultGameplayExperience;
};

#undef UE_API
