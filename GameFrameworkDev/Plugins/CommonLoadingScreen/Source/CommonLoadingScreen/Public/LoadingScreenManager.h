// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "UObject/WeakInterfacePtr.h"

#include "LoadingScreenManager.generated.h"

#define UE_API COMMONLOADINGSCREEN_API

template <typename InterfaceType> class TScriptInterface;

class FSubsystemCollectionBase;
class IInputProcessor;
class ILoadingProcessInterface;
class SWidget;
class UBlackScreenUserWidget;
class ULoadingProgressUserWidget;
class UObject;
class UWorld;
struct FFrame;
struct FWorldContext;

/** 界面加载状态（黑屏与加载界面共用） */
UENUM()
enum class ELoadScreenState : uint8
{
	None,			// 空闲
	PendingLoad,	// 等待加载/显示中
	Loaded,			// 已加载/已显示
	PendingHide		// 等待隐藏中
};

/**
 * 负责加载界面的显示与隐藏
 */
UCLASS(MinimalAPI)
class ULoadingScreenManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~USubsystem 接口
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	UE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~End of USubsystem 接口

	UFUNCTION(BlueprintCallable, Category=LoadingScreen)
	FString GetDebugReasonForShowingOrHidingLoadingScreen() const
	{
		return DebugReasonForShowingOrHidingLoadingScreen;
	}

	/** 返回加载界面当前是否正在显示 */
	UFUNCTION(BlueprintPure, Category = "Loading Screen")
	bool GetLoadingScreenDisplayStatus() const
	{
		return LoadingScreenState != ELoadScreenState::None;
	}

	/** 当加载界面可见性发生变化时调用 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLoadingScreenVisibilityChangedDelegate, bool);
	FORCEINLINE FOnLoadingScreenVisibilityChangedDelegate& OnLoadingScreenVisibilityChangedDelegate() { return LoadingScreenVisibilityChanged; }

	UE_API void RegisterLoadingProcessor(TScriptInterface<ILoadingProcessInterface> Interface);
	UE_API void UnregisterLoadingProcessor(TScriptInterface<ILoadingProcessInterface> Interface);
	
	/** 返回黑屏界面当前是否正在显示 */
	UFUNCTION(BlueprintPure, Category = "Loading Screen|Black Screen")
	bool GetBlackScreenDisplayStatus() const
	{
		return LoadBlackScreenState != ELoadScreenState::None;
	}

	/** 返回任意界面（黑屏或加载界面）当前是否正在显示 */
	UFUNCTION(BlueprintPure, Category = "Loading Screen")
	bool GetAnyScreenDisplayStatus() const
	{
		return GetBlackScreenDisplayStatus() || GetLoadingScreenDisplayStatus();
	}

	/** 当黑屏界面可见性发生变化时调用 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBlackScreenVisibilityChangedDelegate, bool);
	FORCEINLINE FOnBlackScreenVisibilityChangedDelegate& OnBlackScreenVisibilityChangedDelegate() { return BlackScreenVisibilityChanged; }

	/** 返回当前 PreLoadMap 的目标地图名称 */
	UFUNCTION(BlueprintPure, Category=LoadingScreen)
	FString GetPreLoadMapName() const { return PreLoadMapName; }

	/** 加载黑屏界面 */
	UFUNCTION(BlueprintCallable, Category = "Loading Screen")
	UE_API void LoadBlackScreen();

	/** 加载加载界面 */
	UFUNCTION(BlueprintCallable, Category = "Loading Screen")
	UE_API void LoadLoadingScreen();

	/** 准备隐藏黑屏界面 */
	UFUNCTION(BlueprintCallable, Category = "Loading Screen")
	UE_API void PrepareHideBlackScreen();

	/** 准备隐藏加载界面 */
	UFUNCTION(BlueprintCallable, Category = "Loading Screen")
	UE_API void PrepareHideLoadingScreen();

private:
	UE_API void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	UE_API void HandlePostLoadMap(UWorld* World);

	/** 黑屏界面或加载界面是否处于缓动中 */
	bool IsAnyScreenFading() const;

	/** 返回系统/引擎条件是否需要黑屏。 */
	UE_API bool CheckForSystemNeedBlackScreen();

	/** 返回是否有 ILoadingProcessInterface 需要显示 UMG 加载界面控件。 */
	UE_API bool CheckForAnyLoadingProcessInterfaceNeed();

	/** 返回是否想要显示黑屏（系统级，带保持延迟）。 */
	UE_API bool ShouldShowBlackScreen();

	/** 返回是否想要显示 UMG 加载界面控件（仅 ILoadingProcessInterface 条件）。 */
	UE_API bool ShouldShowLoadingScreenWidget();

	/** 返回是否处于初始加载流程中，此时不应使用本界面 */
	UE_API bool IsShowingInitialLoadingScreen() const;

	/** 显示加载界面，在视口上创建加载界面控件 */
	UE_API void ShowLoadingScreen();

	/** 隐藏加载界面，加载界面控件将被销毁 */
	UE_API void HideLoadingScreen();

	/** 在视口上显示黑屏控件 */
	UE_API void ShowBlackScreen();

	/** 隐藏并销毁黑屏控件 */
	UE_API void HideBlackScreen();

	/** 从视口中移除加载界面控件 */
	UE_API void RemoveLoadingScreenWidgetFromViewport();

	/** 从视口中移除黑屏控件 */
	UE_API void RemoveBlackScreenWidgetFromViewport();

	/** 黑屏淡出动画完成后的最终清理（移除控件、恢复性能设置、广播可见性） */
	UE_API void FinishBlackScreenCleanup();

	/** 加载界面遮罩淡出完成后的最终清理（移除控件、恢复性能设置、广播可见性） */
	UE_API void FinishLoadingScreenCleanup();

	/** 黑屏加载动画完成的回调 */
	UFUNCTION()
	void HandleBlackScreenLoadAnimationCompleted();

	/** 黑屏卸载动画完成的回调 */
	UFUNCTION()
	void HandleBlackScreenUnloadAnimationCompleted();

	/** 加载界面加载动画完成的回调 */
	UFUNCTION()
	void HandleLoadingScreenLoadAnimationCompleted();

	/** 加载界面卸载动画完成的回调 */
	UFUNCTION()
	void HandleLoadingScreenUnloadAnimationCompleted();

	/** 加载界面可见时阻止游戏内输入 */
	UE_API void StartBlockingInputForLoadingScreen();

	/** 恢复被加载界面阻止的输入 */
	UE_API void StopBlockingInputForLoadingScreen();

	/** 黑屏可见时阻止输入 */
	UE_API void StartBlockingInputForBlackScreen();

	/** 恢复被黑屏阻止的输入 */
	UE_API void StopBlockingInputForBlackScreen();

	UE_API void ChangePerformanceSettings(bool bEnabingLoadingScreen);
	UE_API void ChangePerformanceSettingsForBlackScreen(bool bEnablingBlackScreen);

private:
	/** 加载界面可见性变化时广播的委托 */
	FOnLoadingScreenVisibilityChangedDelegate LoadingScreenVisibilityChanged;

	/** 黑屏界面可见性变化时广播的委托 */
	FOnBlackScreenVisibilityChangedDelegate BlackScreenVisibilityChanged;

	/** 当前显示的加载界面控件引用（如果存在） */
	TSharedPtr<SWidget> LoadingScreenWidget;

	/** 当前显示的黑屏控件引用（如果存在） */
	TSharedPtr<SWidget> BlackScreenWidget;

	/** 黑屏 UserWidget 的引用，用于控制动画生命周期 */
	UPROPERTY()
	TObjectPtr<UBlackScreenUserWidget> BlackScreenUserWidgetPtr;

	/** 加载界面 UserWidget 的引用，用于控制动画生命周期 */
	UPROPERTY()
	TObjectPtr<ULoadingProgressUserWidget> LoadingScreenUserWidgetPtr;

	/** 加载界面显示时拦截所有输入的输入处理器 */
	TSharedPtr<IInputProcessor> InputPreProcessor;

	/** 黑屏显示时拦截所有输入的输入处理器 */
	TSharedPtr<IInputProcessor> BlackScreenInputPreProcessor;

	/** 外部加载处理器，可能是延迟加载的组件或 Actor。 */
	TArray<TWeakInterfacePtr<ILoadingProcessInterface>> ExternalLoadingProcessors;

	/** 加载界面显示（或隐藏）的原因 */
	FString DebugReasonForShowingOrHidingLoadingScreen;

	/** 黑屏显示（或隐藏）的原因 */
	FString DebugReasonForBlackScreen;

	/** 开始显示加载界面的时间 */
	double TimeLoadingScreenShown = 0.0;

	/** 开始显示黑屏的时间 */
	double TimeBlackScreenShown = 0.0;

	/** 加载界面最近一次想要被关闭的时间（可能因最小显示时长要求而仍在显示） **/
	double TimeLoadingScreenLastDismissed = -1.0;

	/** 黑屏系统条件解除的时间戳（秒），-1.0 表示仍处于活跃状态。用于计算 hold 延迟，在 HoldBlackScreenAdditionalSecs 内继续显示黑屏以等待纹理流式加载完成 **/
	double TimeBlackScreenLastDismissed = -1.0;

	/** 距离下次输出加载界面保持原因的日志还有多少秒 */
	double TimeUntilNextLogHeartbeatSeconds = 0.0;

	/** 加载界面白名单累计检测帧数 */
	int32 LoadingScreenWhitelistAccumulatedFrames = 0;

	/** 当前 PreLoadMap 的目标地图名称 */
	FString PreLoadMapName;

	/** 是否处于 PreLoadMap 与 PostLoadMap 之间 */
	bool bCurrentlyInLoadMap = false;

	/** 当前黑屏加载状态 */
	ELoadScreenState LoadBlackScreenState = ELoadScreenState::None;

	/** 当前加载界面状态 */
	ELoadScreenState LoadingScreenState = ELoadScreenState::None;

	/** 替代 FTickableGameObject，由 FTSTicker 驱动（暂停时仍运行） */
	FTSTicker::FDelegateHandle TickerHandle;
	void StartTicker();
	void StopTicker();
	void ManagerTick(float DeltaTime);
};

#undef UE_API
