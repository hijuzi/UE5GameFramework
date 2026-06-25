// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "PSOCacheManager.generated.h"

class UMaterial;
class UNiagaraSystem;

/**
 * PSO 缓存管理器
 * 负责 PSO（Pipeline State Object）缓存的加载、管理和状态追踪
 */
UCLASS(Config = Game)
class PSOCACHESYSTEM_API UPSOCacheManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 初始化子系统 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 反初始化子系统 */
	virtual void Deinitialize() override;

	/** 检查 PSO 缓存是否就绪 */
	UFUNCTION(BlueprintCallable, Category = "PSOCache")
	bool IsPSOCacheReady() const;

	/** 获取当前 PSO 缓存总数 */
	UFUNCTION(BlueprintCallable, Category = "PSOCache")
	int32 GetCachedPSOCount() const;

	/** 获取项目中所有 Material 资源 */
	UFUNCTION(BlueprintCallable, Category = "PSOCache")
	static TArray<TSoftObjectPtr<UMaterial>> GetAllMaterials();

	/** 获取项目中所有 Niagara 粒子系统资源 */
	UFUNCTION(BlueprintCallable, Category = "PSOCache")
	static TArray<TSoftObjectPtr<UNiagaraSystem>> GetAllNiagaraParticleSystems();

	/** 加载内建 PSO 缓存 */
	void LoadBuiltInPSOCache();

	/** 清除所有已缓存的 PSO */
	void ClearPSOCache();

protected:
	/** PSO 缓存是否已就绪 */
	bool bPSOCacheReady = false;

	/** 当前缓存的 PSO 数量 */
	int32 CachedPSOCount = 0;
};
