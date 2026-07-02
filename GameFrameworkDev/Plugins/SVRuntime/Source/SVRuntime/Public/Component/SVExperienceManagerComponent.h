// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "ControlFlowNode.h"
#include "LevelLoading/LevelLoadingManager.h"

#include "SVExperienceManagerComponent.generated.h"

#define UE_API SVRUNTIME_API

class FControlFlow;
class FString;
class UCommonActivatableWidget;

/**
 * USVExperienceManagerComponent
 *
 * ActorComponent，负责管理 Experience 加载后的前端流程和 UI 界面。
 * 使用 FControlFlow 管理流程步骤，从 SVBaseExperienceDefinition / SVLoginExperienceDefinition 读取 UI 类配置。
 */
UCLASS(MinimalAPI)
class USVExperienceManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USVExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface



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

	/** 获取 LevelLoadingManager 子系统 */
	class ULevelLoadingManager* GetLevelLoadingManager() const;

	/** 检查 LevelLoadingScreen 是否正在显示 */
	bool IsLevelLoadingScreenDisplayed() const;

	/** 前端的 FControlFlow 实例，管理流程步骤 */
	TSharedPtr<FControlFlow> FrontEndFlow;

	/** 正在进行的 Press Start 界面任务 */
	FControlFlowNodePtr InProgressPressStartScreen;

	// ---- BeginPlay 等待屏幕隐藏状态 ----
	/** BeginPlay 等待期间 LevelLoadingScreen 可见性委托 Handle */
	FDelegateHandle BeginPlayWait_LSHandle;

	/** BeginPlay 等待是否已完成（防重入） */
	bool bBeginPlayWaitCompleted = false;

	/** 着色器编译界面类（从 SVLoginExperienceDefinition 读取） */
	TSoftClassPtr<UCommonActivatableWidget> CompilingShadersScreenClass;

	/** 是否强制开启着色器编译界面（从 SVLoginExperienceDefinition 读取） */
	bool bForceShowCompilingShadersScreen = false;

	/** 编辑器中是否也需要开启着色器界面（从 SVLoginExperienceDefinition 读取） */
	bool bEnableCompilingShadersInEditor = false;

	/** Press Start 界面类（从 SVLoginExperienceDefinition 读取） */
	TSoftClassPtr<UCommonActivatableWidget> PressStartScreenClass;

	/** 主界面类（从 SVBaseExperienceDefinition 读取） */
	TSoftClassPtr<UCommonActivatableWidget> MainScreenClass;
};

#undef UE_API
