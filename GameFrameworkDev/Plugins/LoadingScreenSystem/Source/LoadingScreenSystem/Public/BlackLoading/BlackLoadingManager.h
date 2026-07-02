// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "BlackLoadingManager.generated.h"

template <typename InterfaceType> class TScriptInterface;

class FSubsystemCollectionBase;
class IInputProcessor;
class IBlackLoadingProcessInterface;
class SWidget;
class UBlackLoadingProcessTask;
class UBlackLoadingScreenWidget;
class UObject;
class UWorld;
struct FFrame;

/** 黑屏加载界面可见性变化委托 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBlackLoadingScreenVisibilityChanged, bool /* bIsVisible */);

UCLASS(MinimalAPI)
class UBlackLoadingManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~ End of USubsystem interface

	/** 每帧更新加载界面状态 */
	bool Tick(float DeltaTime);

	/** 加载界面是否正在显示 */
	bool GetBlackLoadingScreenDisplayStatus() const
	{
		return bCurrentlyShowingBlackLoadingScreen;
	}

	/** 黑屏加载界面是否正在播放动画 */
	bool IsBlackLoadingScreenAnimationPlaying() const;

	/** 注册外部黑屏加载处理器 */
	void RegisterBlackLoadingProcessor(TScriptInterface<IBlackLoadingProcessInterface> Interface);

	/** 取消注册外部黑屏加载处理器 */
	void UnregisterBlackLoadingProcessor(TScriptInterface<IBlackLoadingProcessInterface> Interface);

	/** 打开黑屏加载界面（先销毁已有任务，再创建新任务） */
	UFUNCTION(BlueprintCallable, Category = "LoadingScreen")
	void OpenBlackLoadingScreen(const FString& Reason, bool bAutoClose = false);

	/** 关闭黑屏加载界面 */
	UFUNCTION(BlueprintCallable, Category = "LoadingScreen")
	void CloseBlackLoadingScreen(const FString& Reason);

	/** 黑屏加载界面可见性变化事件 */
	FOnBlackLoadingScreenVisibilityChanged BlackLoadingScreenVisibilityChanged;

private:
	/** 每帧检查并更新黑屏加载界面 */
	void UpdateBlackLoadingScreen();

	/** 检查是否有任何原因需要显示黑屏加载界面 */
	bool CheckForAnyNeedToShowBlackLoadingScreen();

	/** 判断是否应该显示黑屏加载界面 */
	bool ShouldShowBlackLoadingScreen();

	/** 是否正在显示初始加载界面（引擎预加载界面） */
	bool IsShowingInitialBlackLoadingScreen() const;

	/** 显示黑屏加载界面 */
	void ShowBlackLoadingScreen();

	/** 隐藏黑屏加载界面 */
	void HideBlackLoadingScreen();

	/** 完成黑屏加载界面的最终清理（动画驱动的卸载流程完成后调用） */
	void FinishBlackLoadingScreenCleanup();

	/** 从视口移除 Widget */
	void RemoveBlackLoadingScreenWidgetFromViewport();

	/** 拦截输入 */
	void StartBlockingInput();

	/** 恢复输入 */
	void StopBlockingInput();

	/** 加载动画完成回调 */
	UFUNCTION()
	void HandleLoadingScreenLoadAnimationCompleted();

	/** 卸载动画完成回调 */
	UFUNCTION()
	void HandleLoadingScreenUnloadAnimationCompleted();

private:
	/** 当前显示的加载界面 Widget 实例 */
	TObjectPtr<UBlackLoadingScreenWidget> BlackLoadingScreenUserWidgetPtr;

	/** 当前显示的加载界面 SWidget */
	TSharedPtr<SWidget> BlackLoadingScreenWidget;

	/** 输入拦截器 */
	TSharedPtr<IInputProcessor> InputPreProcessor;

	/** Ticker 句柄 */
	FTSTicker::FDelegateHandle TickerHandle;

	/** 外部注册的黑屏加载处理器 */
	TArray<TWeakInterfacePtr<IBlackLoadingProcessInterface>> ExternalBlackLoadingProcessors;

	/** 由 OpenBlackLoadingScreen 创建的黑屏加载任务 */
	UPROPERTY()
	TObjectPtr<UBlackLoadingProcessTask> BlackLoadingProcessTask;

	/** 当前正在显示加载界面 */
	bool bCurrentlyShowingBlackLoadingScreen = false;

	/** 是否自动关闭黑屏界面（开启后 BlackLoadingProcessTask 不再阻止关闭） */
	bool bAutoCloseBlackLoadingScreen = false;

	/** 显示加载界面的原因说明（调试用） */
	FString DebugReasonForShowingOrHidingBlackLoadingScreen;

	/** 加载界面显示的时间戳 */
	double TimeBlackLoadingScreenShown = 0.0;
};
