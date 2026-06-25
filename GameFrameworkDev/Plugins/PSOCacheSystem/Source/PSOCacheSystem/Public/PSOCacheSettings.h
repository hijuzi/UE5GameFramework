// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "UObject/SoftObjectPath.h"

#include "PSOCacheSettings.generated.h"

/**
 * 材质覆盖策略配置
 */
USTRUCT(BlueprintType)
struct PSOCACHESYSTEM_API FPSOMaterialCoverageConfig
{
	GENERATED_BODY()

	/** 每帧覆盖的材质数量（建议 1~5，避免阻塞主线程） */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage", meta = (ClampMin = "1", ClampMax = "50"))
	int32 MaterialsPerFrame = 2;

	/** 每个材质在网格上的渲染时间（秒） */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage", meta = (ForceUnits = "s", ClampMin = "0.1", ClampMax = "5.0"))
	float MaterialRenderDuration = 0.5f;

	/** 每个材质切换后的 FlushRendering 等待时间（秒） */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage", meta = (ForceUnits = "s", ClampMin = "0.01", ClampMax = "2.0"))
	float FlushWaitDuration = 0.1f;

	/** 是否覆盖 SkeletalMesh（骨架网格体） */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage")
	bool bCoverSkeletalMesh = true;

	/** 是否覆盖 Landscape（地形） */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage")
	bool bCoverLandscape = true;

	/** 是否覆盖 ISM/HISM（实例化网格） */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage")
	bool bCoverInstancedMesh = true;

	/** 是否覆盖 SplineMesh（样条网格） */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage")
	bool bCoverSplineMesh = false;

	/** 材质 Actor 对象池大小（循环复用，默认 10000） */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage|Pool", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 MaterialActorPoolSize = 10000;

	/** 材质 Actor 网格单元格大小（单位：厘米，决定网格间距） */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage|Pool", meta = (ForceUnits = "cm", ClampMin = "100", ClampMax = "50000"))
	float MaterialGridCellSize = 1000.0f;
};

/**
 * Niagara 粒子覆盖策略配置
 */
USTRUCT(BlueprintType)
struct PSOCACHESYSTEM_API FPSONiagaraCoverageConfig
{
	GENERATED_BODY()

	/** 每帧覆盖的 Niagara 系统数量 */
	UPROPERTY(config, EditAnywhere, Category = "Niagara Coverage", meta = (ClampMin = "1", ClampMax = "20"))
	int32 SystemsPerFrame = 1;

	/** 每个 Niagara 系统的播放时长（秒），需足够触发所有 Emitter */
	UPROPERTY(config, EditAnywhere, Category = "Niagara Coverage", meta = (ForceUnits = "s", ClampMin = "1.0", ClampMax = "10.0"))
	float NiagaraPlayDuration = 3.0f;

	/** 每个 Niagara 播放后的等待异步编译时间（秒） */
	UPROPERTY(config, EditAnywhere, Category = "Niagara Coverage", meta = (ForceUnits = "s", ClampMin = "0.1", ClampMax = "5.0"))
	float NiagaraWaitDuration = 1.0f;

	/** 是否覆盖 GPU Simulation 粒子 */
	UPROPERTY(config, EditAnywhere, Category = "Niagara Coverage")
	bool bCoverGPUSimulation = true;

	/** 是否覆盖 CPU Simulation 粒子 */
	UPROPERTY(config, EditAnywhere, Category = "Niagara Coverage")
	bool bCoverCPUSimulation = true;

	/** 是否覆盖世界空间粒子 */
	UPROPERTY(config, EditAnywhere, Category = "Niagara Coverage")
	bool bCoverWorldSpace = true;

	/** Niagara Actor 对象池大小（循环复用，默认 10000） */
	UPROPERTY(config, EditAnywhere, Category = "Niagara Coverage|Pool", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 NiagaraActorPoolSize = 10000;

	/** Niagara Actor 网格单元格大小（单位：厘米，决定网格间距） */
	UPROPERTY(config, EditAnywhere, Category = "Niagara Coverage|Pool", meta = (ForceUnits = "cm", ClampMin = "100", ClampMax = "50000"))
	float NiagaraGridCellSize = 1000.0f;
};

/**
 * PSO 缓存系统配置
 * 路径：Project Settings → Plugins → PSO Cache
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="PSO Cache"))
class PSOCACHESYSTEM_API UPSOCacheSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	UPSOCacheSettings();

public:
	//~=========================================================================
	// 通用
	//~=========================================================================

	/** PSO 覆盖策略所使用的 Warmup 地图路径 */
	UPROPERTY(config, EditAnywhere, Category = "General", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath PSOCoverageMap;

	/** 覆盖策略全部完成后延迟多少秒才关闭（单位：秒，给编译异步完成留缓冲） */
	UPROPERTY(config, EditAnywhere, Category = "General", meta = (ForceUnits = "s", ClampMin = "0.0", ClampMax = "1000.0"))
	float CoverageCompleteDelaySeconds = 180.0f;

	//~=========================================================================
	// 材质覆盖
	//~=========================================================================

	/** 是否开启材质覆盖策略 */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage")
	bool bEnableMaterialCoverage = true;

	/** 材质覆盖策略子配置（仅在 bEnableMaterialCoverage 为 true 时生效） */
	UPROPERTY(config, EditAnywhere, Category = "Material Coverage", meta = (EditCondition = "bEnableMaterialCoverage"))
	FPSOMaterialCoverageConfig MaterialCoverageConfig;

	//~=========================================================================
	// Niagara 覆盖
	//~=========================================================================

	/** 是否开启 Niagara 粒子覆盖策略 */
	UPROPERTY(config, EditAnywhere, Category = "Niagara Coverage")
	bool bEnableNiagaraCoverage = true;

	/** Niagara 覆盖策略子配置（仅在 bEnableNiagaraCoverage 为 true 时生效） */
	UPROPERTY(config, EditAnywhere, Category = "Niagara Coverage", meta = (EditCondition = "bEnableNiagaraCoverage"))
	FPSONiagaraCoverageConfig NiagaraCoverageConfig;
};
