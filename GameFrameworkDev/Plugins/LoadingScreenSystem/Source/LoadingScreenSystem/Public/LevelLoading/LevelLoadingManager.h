// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Widgets/SWidget.h"

#include "LoadingScreenSettings.h"

#include "LevelLoadingManager.generated.h"

/** 关卡加载界面可见性变化委托 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLevelLoadingScreenVisibilityChanged, bool /* bIsVisible */);

class IInputProcessor;
class ILevelLoadingScreenInterface;
class ULevelLoadingScreenWidget;
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
UCLASS()
class LOADINGSCREENSYSTEM_API ULevelLoadingManager : public UGameInstanceSubsystem
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

	/** 关卡加载界面是否正在处于常驻打开状态 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Loading|Progress")
	bool IsLevelLoadingScreenPersistent() const;

	/** 当前是否正在加载关卡中，并查表确认是否需要显示加载界面（PreLoadMap 与 PostLoadMap 之间） */
	bool IsCurrentlyLoadingMap();

	/** 当前加载的目标地图名称 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Loading|Progress")
	const FString& GetPreLoadMapName() const { return PreLoadMapName; }

	/** 获取当前加载关卡的覆盖配置（从 DataTable 中查表，未配置时返回默认空配置） */
	UFUNCTION(BlueprintCallable, Category = "Loading|Config")
	FLevelLoadingScreenOverrideConfig GetCurrentLevelOverrideConfig();

	/** 关卡加载界面可见性变化事件 */
	FOnLevelLoadingScreenVisibilityChanged LevelLoadingScreenVisibilityChanged;

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

	/** 每帧更新加载界面状态（初始化时注册，销毁时移除） */
	bool Tick(float DeltaTime);

	/** 驱动进度计算（由 Tick 根据 bIsProgressTickEnabled 控制触发） */
	void TickProgress(float DeltaTime);

	/** 准备开始加载（Preparing 阶段 Ticker 逻辑，检测异步加载启动） */
	void OnLoadingStarted();

	/** 加载完成时的清理和日志输出 */
	void OnLoadingCompleted();

	// ================================================================
	// Level Loading Screen UI
	// ================================================================

	/** 根据加载状态更新关卡加载界面的显示/隐藏 */
	void UpdateLevelLoadingScreen();

	/** 是否需要显示关卡加载界面（含命令行/视口检查 + 需求判断 + 额外保持时长） */
	bool ShouldShowLevelLoadingScreen();

	/** 检查是否有任何需要显示关卡加载界面的原因（含 World/GameState/加载状态等综合判断） */
	bool CheckForAnyNeedToShowLevelLoadingScreen();

	/** 是否正在显示初始加载界面（引擎预加载界面） */
	bool IsShowingInitialLevelLoadingScreen() const;

	/** 创建并显示关卡加载界面 */
	void ShowLevelLoadingScreen();

	/** 隐藏并销毁关卡加载界面 */
	void HideLevelLoadingScreen();

	/** 完成关卡加载界面的最终清理（动画驱动的卸载流程完成后调用） */
	void FinishLevelLoadingScreenCleanup();

	/** 从视口移除关卡加载界面 Widget */
	void RemoveLevelWidgetFromViewport();

	/** 拦截输入 */
	void StartBlockingInput();

	/** 恢复输入 */
	void StopBlockingInput();

	/** 调整性能设置（ShaderPipelineCache / 世界渲染 / 流式加载优先级 / 挂起检测等） */
	void ChangePerformanceSettings(bool bEnableLevelLoadingScreen);

	/** 加载动画完成回调 */
	UFUNCTION()
	void HandleLevelLoadingScreenLoadAnimationCompleted();

	/** 卸载动画完成回调 */
	UFUNCTION()
	void HandleLevelLoadingScreenUnloadAnimationCompleted();

	// ================================================================
	// Map Loading Callbacks
	// ================================================================

	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** 根据地图名称查表，返回匹配的行（无匹配返回 nullptr） */
	const FLevelLoadingScreenTableRow* FindLevelLoadingScreenTableRow(const FString& MapName);

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

	/** Ticker 句柄，驱动内部进度计算（生命周期：Initialize 注册，Deinitialize 移除） */
	FTSTicker::FDelegateHandle TickerHandle;

	/** 是否启用进度 Tick */
	bool bIsProgressTickEnabled = false;

	/** Ticker 缓存的最新进度值，GetPreciseLoadingProgress 直接返回（无广播开销） */
	float CachedProgress = 100.0f;

	/** 最近一次关卡的引擎加载耗时（PreLoadMap → PostLoadMap，仅包加载，不含 WorldInit/Finalizing） */
	float LevelLoadingDuration = 0.0f;

	/** 关卡加载界面最小显示时长（秒），从 LoadingScreenSettings 读取 */
	float MinimumLevelLoadingScreenDisplayTimeSecs = 2.0f;

	/** The reason why the loading screen is up (or not) */
	FString DebugReasonForShowingOrHidingLevelLoadingScreen;

	/** 关卡加载界面 UWidget 实例 */
	TObjectPtr<ULevelLoadingScreenWidget> LevelLoadingScreenUserWidgetPtr;

	/** 关卡加载界面 SWidget 引用 */
	TSharedPtr<SWidget> LevelLoadingScreenWidget;

	/** 输入拦截器 */
	TSharedPtr<IInputProcessor> InputPreProcessor;

	/** 关卡加载界面当前是否正在显示 */
	bool bCurrentlyShowingLevelLoadingScreen = false;

	/** 最后一次不再需要显示加载界面的时间（用于额外保持） */
	double TimeLoadingScreenLastDismissed = -1.0;

	/** 加载界面显示的时间戳 */
	double TimeLevelLoadingScreenShown = 0.0;

	/** 关卡加载界面覆盖配置 DataTable 缓存（懒加载，避免每次查表都 TryLoad） */
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> CachedLevelLoadingScreenOverrideTable;
};
