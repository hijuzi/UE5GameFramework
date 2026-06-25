// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "ControlFlowNode.h"
#include "LoadingProcessInterface.h"

#include "SVExperienceManagerComponent.generated.h"

#define UE_API SVRUNTIME_API

class FControlFlow;
class FString;
class UCommonActivatableWidget;

/**
 * USVExperienceManagerComponent
 *
 * ActorComponent，负责管理 Experience 加载后的前端流程和 UI 界面。
 * 实现 ILoadingProcessInterface，在流程进行期间控制 Loading Screen 的显示。
 * 使用 FControlFlow 管理流程步骤，从 SVBaseExperienceDefinition / SVLoginExperienceDefinition 读取 UI 类配置。
 */
UCLASS(MinimalAPI)
class USVExperienceManagerComponent : public UActorComponent, public ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	USVExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	//~ Begin ILoadingProcessInterface
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;
	virtual void GetLoadingScreenOverrideConfig(FLoadingScreenOverrideConfig& OutConfig) const override;
	//~ End ILoadingProcessInterface

protected:
	/** Experience 加载完成后的回调，读取 UI 配置并启动 FlowStep 流程。 */
	virtual void OnExperienceLoaded();

	/** 通用辅助：尝试将 Widget 推入全屏层。
	 * @param bWaitForDeactivation 是否等待 Widget 反激活后再继续流程（MainScreen 为 false，其余为 true） */
	void TryPushWidgetToLayer(FControlFlowNodeRef SubFlow, const TSoftClassPtr<UCommonActivatableWidget>& ScreenClass, bool bWaitForDeactivation);

	/** BeginPlay 时检查是否有界面正在显示，若无则直接启动 Experience，否则注册监听等待隐藏后再启动 */
	void WaitForScreensHiddenThenStartExperience();

	/** BeginPlay 等待回调：任意屏幕可见性变化时触发 */
	UFUNCTION()
	void OnBeginPlayWaitScreenHidden(bool bIsVisible);

	/** 反注册 BeginPlay 等待屏幕隐藏的委托绑定 */
	void UnregisterBeginPlayWaitScreenEvents();

	/** 注册 BeginPlay 等待屏幕隐藏的委托绑定 */
	void RegisterBeginPlayWaitScreenEvents();

	/** Flow Step: 尝试显示着色器编译界面（如果 PSO 已全部编译完成则跳过） */
	void FlowStep_TryShowCompilingShadersScreen(FControlFlowNodeRef SubFlow);
	/** Flow Step: 尝试显示 Press Start 界面 */
	void FlowStep_TryShowPressStartScreen(FControlFlowNodeRef SubFlow) { TryPushWidgetToLayer(SubFlow, PressStartScreenClass, true); }
	/** Flow Step: 尝试显示主界面 */
	void FlowStep_TryShowMainScreen(FControlFlowNodeRef SubFlow) { TryPushWidgetToLayer(SubFlow, MainScreenClass, false); }

	/** 查询 WorldSettings，返回该关卡是否使用 Loading Screen */
	bool ShouldUseLoadingScreen() const;

	/** 获取 LoadingScreenManager 子系统，可能返回 nullptr */
	class ULoadingScreenManager* GetLoadingScreenManager() const;

	/** 是否应该显示 Loading Screen */
	bool bShouldShowLoadingScreen = true;

	/** 前端的 FControlFlow 实例，管理流程步骤 */
	TSharedPtr<FControlFlow> FrontEndFlow;

	/** 正在进行的 Press Start 界面任务 */
	FControlFlowNodePtr InProgressPressStartScreen;

	// ---- BeginPlay 等待屏幕隐藏状态 ----
	/** BeginPlay 等待期间 LoadingScreen 可见性委托 Handle */
	FDelegateHandle BeginPlayWait_LSHandle;

	/** BeginPlay 等待期间 BlackScreen 可见性委托 Handle */
	FDelegateHandle BeginPlayWait_BSHandle;

	/** BeginPlay 等待是否已完成（防重入） */
	bool bBeginPlayWaitCompleted = false;

	/** 着色器编译界面类（从 SVLoginExperienceDefinition 读取） */
	TSoftClassPtr<UCommonActivatableWidget> CompilingShadersScreenClass;

	/** Press Start 界面类（从 SVLoginExperienceDefinition 读取） */
	TSoftClassPtr<UCommonActivatableWidget> PressStartScreenClass;

	/** 主界面类（从 SVBaseExperienceDefinition 读取） */
	TSoftClassPtr<UCommonActivatableWidget> MainScreenClass;
};

#undef UE_API
