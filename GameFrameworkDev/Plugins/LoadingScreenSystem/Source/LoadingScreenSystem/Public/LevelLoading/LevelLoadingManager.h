// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"

#include "LevelLoadingManager.generated.h"

#define UE_API LOADINGSCREENSYSTEM_API

DECLARE_LOG_CATEGORY_EXTERN(LogLevelLoading, Log, All);

class ULevelStreaming;
struct FWorldContext;

/**
 * 关卡加载阶段定义
 * 根据 OpenLevel 的完整管线拆分为多阶段，精确反馈加载进度
 *
 * 完整流程：
 *   OpenLevel 调用
 *     ├── [Preparing]     PreLoadMap 触发，卸载旧World —— 0% ~ 5%
 *     ├── [AsyncLoading]  异步加载 Package —— 5% ~ 70%  (引擎真实进度)
 *     ├── [WorldInit]     World BeginPlay / Actor 生成 —— 70% ~ 95%  (时间平滑估算)
 *     └── [Finalizing]    收尾等待 / 最小显示时长 —— 95% ~ 100%
 */
UENUM(BlueprintType)
enum class ELevelLoadingPhase : uint8
{
	None            UMETA(DisplayName = "无"),
	Preparing       UMETA(DisplayName = "准备中"),       // PreLoadMap 后，卸载旧关卡
	AsyncLoading    UMETA(DisplayName = "加载资源"),     // 异步包加载中（引擎 GetAsyncLoadPercentage）
	WorldInit       UMETA(DisplayName = "初始化世界"),   // World BeginPlay / Spawn Actor 等
	Finalizing      UMETA(DisplayName = "即将完成"),     // 收尾阶段 / 最小显示时长等待
	Completed       UMETA(DisplayName = "已完成"),
};

/**
 * 关卡加载管理器
 *
 * 职责：精确追踪关卡加载进度，将 OpenLevel 的完整管线拆分为多阶段并分别估算。
 * 与 ULoadingScreenManager 解耦——本管理器只负责进度计算，不管理 UI 显示。
 *
 * 使用方式：
 *   1. 自动订阅 PreLoadMap / PostLoadMap，无需手动初始化
 *   2. GetPreciseLoadingProgress() 获取 0~100 的精确进度值（轮询模式，无广播）
 */
UCLASS(MinimalAPI)
class ULevelLoadingManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ================================================================
	// USubsystem Interface
	// ================================================================
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ================================================================
	// Public API
	// ================================================================

	/** 获取多阶段精确加载进度 (0.0 ~ 100.0) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Loading|Progress")
	float GetPreciseLoadingProgress() const;

	/** 获取当前加载阶段（UI 可据此切换提示文案） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Loading|Progress")
	ELevelLoadingPhase GetCurrentLoadingPhase() const;

	/** 引擎原生异步加载进度（-1 = 已完成 / 未开始） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Loading|Progress")
	float GetRawAsyncLoadPercentage() const;

	/** 是否处于关卡加载中 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Loading|Progress")
	bool IsLoadingInProgress() const;

	/** 准备关闭关卡加载界面，跳过剩余等待直接进入收尾阶段 */
	UFUNCTION(BlueprintCallable, Category = "Loading|Progress")
	void PrepareCloseLoadingScreen();

	/** 当前加载的目标地图名称 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Loading|Progress")
	const FString& GetPreLoadMapName() const { return PreLoadMapName; }

	// ================================================================
	// Configuration (EditDefaultsOnly / BlueprintReadWrite)
	// ================================================================

	/** 各阶段权重占比（总计应为 100） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading|Progress")
	float PreparingWeight   = 5.0f;     // ~ 0%  ~ 5%

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading|Progress")
	float AsyncLoadWeight   = 65.0f;    // ~ 5%  ~ 70%

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading|Progress")
	float WorldInitWeight   = 25.0f;    // ~ 70% ~ 95%

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading|Progress")
	float FinalizeWeight    = 5.0f;     // ~ 95% ~ 100%

	/** Ticker 更新间隔（秒），驱动内部进度计算 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading|Progress")
	float ProgressTickInterval = 0.1f;

private:
	// ================================================================
	// Phase Management
	// ================================================================

	void SetLoadingPhase(ELevelLoadingPhase NewPhase);
	float CalculatePhaseProgress() const;

	/** Ticker 回调：自动切换阶段并缓存最新进度 */
	void TickProgress();

	// ================================================================
	// Map Loading Callbacks
	// ================================================================

	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	// ================================================================
	// Member Variables
	// ================================================================

	/** 当前加载阶段 */
	ELevelLoadingPhase CurrentLoadingPhase = ELevelLoadingPhase::None;

	/** 阶段切换时间戳 */
	double PhaseStartTime = 0.0;

	/** 关卡加载开始时间 */
	double LoadingStartTime = 0.0;

	/** 当前 PreLoadMap 的目标地图名称 */
	FString PreLoadMapName;

	/** 是否处于 PreLoadMap 与 PostLoadMap 之间 */
	bool bCurrentlyInLoadMap = false;

	/** Ticker 句柄，驱动内部进度计算 */
	FTSTicker::FDelegateHandle ProgressTickerHandle;

	/** Ticker 缓存的最新进度值，GetPreciseLoadingProgress 直接返回（无广播开销） */
	float CachedProgress = 100.0f;

	/** 关卡加载界面最小显示时长（秒），从 LoadingScreenSettings 读取 */
	float MinimumLevelLoadingScreenDisplayTimeSecs = 2.0f;
};

#undef UE_API
