// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "PSOCacheAssetList.generated.h"

class UMaterial;
class UNiagaraSystem;

/**
 * PSO 缓存资产列表 DataAsset
 * 存储项目中所有需要用到的 Material 和 NiagaraSystem 软引用列表
 * 供 PSO 预编译 / 打包流程使用
 */
UCLASS(BlueprintType)
class PSOCACHESYSTEM_API UPSOCacheAssetList : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 项目中所有 Material 资源列表 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PSO")
	TArray<TSoftObjectPtr<UMaterial>> Materials;

	/** 项目中所有 Niagara 粒子系统资源列表 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PSO")
	TArray<TSoftObjectPtr<UNiagaraSystem>> NiagaraSystems;

	/** 
	 * 从 AssetRegistry 扫描并刷新资产列表
	 * - 通过 Content Browser 右键菜单触发
	 * - 也可由 Commandlet 在流水线中自动调用
	 */
	UFUNCTION(Category = "PSO")
	void RefreshFromAssetRegistry();
};
