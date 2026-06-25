// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "PSOCacheManager.generated.h"

class UMaterial;
class UNiagaraSystem;
class UNiagaraComponent;
class AStaticMeshActor;
class UPSOCacheMonitorWidget;

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

	/** 获取当前剩余待预编译的 PSO 数量（返回 0 表示全部编译完成） */
	UFUNCTION(BlueprintCallable, Category = "PSOCache")
	int32 GetPrecompilesRemaining() const;

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

	// ---- 自动化覆盖策略 ----

	/** 开始自动化覆盖策略：依次执行材质覆盖 + Niagara粒子覆盖 */
	UFUNCTION(BlueprintCallable, Category = "PSOCache", Exec)
	void BeginAutomatedCoverageStrategy();

	/** 结束自动化覆盖策略 */
	UFUNCTION(BlueprintCallable, Category = "PSOCache", Exec)
	void EndAutomatedCoverageStrategy();

protected:
	/** 覆盖策略每帧 Tick（由 FTSTicker 驱动） */
	bool TickCoverage(float DeltaTime);

	/** 开始材质覆盖策略：遍历全部材质 × 各类网格类型（SM/SKM），强制渲染触发 PSO 编译 */
	void BeginMaterialCoverageStrategy();

	/** 开始 Niagara 粒子覆盖策略：遍历全部 Niagara 系统，逐个 Spawn 播放触发 PSO 编译 */
	void BeginNiagaraCoverageStrategy();

	/** PSO 缓存是否已就绪 */
	bool bPSOCacheReady = false;

	/** 当前缓存的 PSO 数量 */
	int32 CachedPSOCount = 0;

private:
	/** 处理 -psosysautocoverage 启动参数：等待 World 就绪后切入 Warmup 地图并自动开始覆盖 */
	void HandleAutoStartCoverage();

	/** 覆盖策略的阶段 */
	enum class EPSOCoveragePhase : uint8
	{
		Material,	// 材质覆盖
		Niagara,	// Niagara 粒子覆盖
		Complete	// 全部完成
	};

	/** 将单个材质应用到网格并强制渲染，触发 PSO 编译 */
	void ProcessMaterialForPSO(const TSoftObjectPtr<UMaterial>& Material, int32 MaterialIndex);

	/** Spawn 单个 Niagara 系统并播放，触发 PSO 编译 */
	void ProcessNiagaraForPSO(const TSoftObjectPtr<UNiagaraSystem>& NiagaraSystem, int32 NiagaraIndex);

	/** 根据对象池索引计算网格位置 */
	static FVector GetGridPosition(int32 SlotIndex, int32 PoolSize, float CellSize);

	/** 清理所有对象池 Actor */
	void CleanupCoverageActors();

	/** 覆盖策略是否正在运行 */
	bool bCoverageActive = false;

	/** FTSTicker 句柄，用于注册/注销 Tick */
	FTSTicker::FDelegateHandle TickDelegateHandle;

	/** 当前覆盖阶段 */
	EPSOCoveragePhase CoveragePhase = EPSOCoveragePhase::Material;

	/** 覆盖全部完成后的累计等待时间（秒），超过配置的延迟后关闭 */
	float CoverageCompleteElapsed = 0.0f;

	/** 自动化策略从开始到现在的累计时间（秒） */
	float CoverageElapsedTotal = 0.0f;

	/** 监控 UI Widget（UWidget 引用，用于 UpdateDisplay） */
	UPROPERTY()
	TObjectPtr<UPSOCacheMonitorWidget> MonitorWidget;

	/** 监控 UI 对应的 Slate Widget（用于 Add/RemoveViewportWidgetContent） */
	TSharedPtr<SWidget> MonitorSlateWidget;

	/** 显示监控 UI */
	void ShowMonitorUI();

	/** 更新监控 UI 数据 */
	void UpdateMonitorUI();

	/** 关闭监控 UI */
	void HideMonitorUI();

	/** 材质覆盖：当前材质索引 */
	int32 CurrentMaterialIndex = 0;

	/** 材质覆盖：缓存的全部材质列表 */
	TArray<TSoftObjectPtr<UMaterial>> CachedMaterials;

	/** Niagara覆盖：当前粒子系统索引 */
	int32 CurrentNiagaraIndex = 0;

	/** Niagara覆盖：缓存的全部 Niagara 系统列表 */
	TArray<TSoftObjectPtr<UNiagaraSystem>> CachedNiagaraSystems;

	/** 材质覆盖：对象池中的 AStaticMeshActor 列表（懒加载，按网格排列） */
	TArray<TWeakObjectPtr<AStaticMeshActor>> MaterialActorPool;

	/** 材质覆盖：网格单元格大小（用于懒加载时计算 spawn 位置） */
	float MaterialPoolCellSize = 1000.0f;

	/** Niagara覆盖：对象池中的 Actor 列表（懒加载） */
	TArray<TWeakObjectPtr<AActor>> NiagaraActorPool;

	/** Niagara覆盖：对象池中对应的 NiagaraComponent 列表（与 Actor 一一对应） */
	TArray<TWeakObjectPtr<UNiagaraComponent>> NiagaraCompPool;

	/** Niagara覆盖：网格单元格大小（用于懒加载时计算 spawn 位置） */
	float NiagaraPoolCellSize = 1000.0f;

	/** 启动参数 -psosysautocoverage：启动时自动开始覆盖策略 */
	bool bAutoStartCoverage = false;

	/** 启动参数 -psosysautoquitgame：覆盖策略完成后自动退出游戏 */
	bool bAutoQuitGame = false;
};
