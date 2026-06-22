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
	//~ End ILoadingProcessInterface

protected:
	/** Experience 加载完成后的回调，读取 UI 配置并启动 FlowStep 流程。 */
	virtual void OnExperienceLoaded();

	/** 通用辅助：尝试将 Widget 推入全屏层。
	 * @param bWaitForDeactivation 是否等待 Widget 反激活后再继续流程（MainScreen 为 false，其余为 true） */
	void TryPushWidgetToLayer(FControlFlowNodeRef SubFlow, const TSoftClassPtr<UCommonActivatableWidget>& ScreenClass, bool bWaitForDeactivation);

	/** Flow Step: 等待黑屏/加载界面隐藏后再继续 */
	void FlowStep_WaitForAnyScreenHidden(FControlFlowNodeRef SubFlow);

	/** Flow Step: 尝试显示着色器编译界面 */
	void FlowStep_TryShowCompilingShadersScreen(FControlFlowNodeRef SubFlow) { TryPushWidgetToLayer(SubFlow, CompilingShadersScreenClass, true); }
	/** Flow Step: 尝试显示 Press Start 界面 */
	void FlowStep_TryShowPressStartScreen(FControlFlowNodeRef SubFlow) { TryPushWidgetToLayer(SubFlow, PressStartScreenClass, true); }
	/** Flow Step: 尝试显示主界面 */
	void FlowStep_TryShowMainScreen(FControlFlowNodeRef SubFlow) { TryPushWidgetToLayer(SubFlow, MainScreenClass, false); }

	/** 查询 WorldSettings，返回该关卡是否使用 Loading Screen */
	bool ShouldUseLoadingScreen() const;

	/** 是否应该显示 Loading Screen */
	bool bShouldShowLoadingScreen = true;

	/** 前端的 FControlFlow 实例，管理流程步骤 */
	TSharedPtr<FControlFlow> FrontEndFlow;

	/** 正在进行的 Press Start 界面任务 */
	FControlFlowNodePtr InProgressPressStartScreen;

	/** 着色器编译界面类（从 SVLoginExperienceDefinition 读取） */
	TSoftClassPtr<UCommonActivatableWidget> CompilingShadersScreenClass;

	/** Press Start 界面类（从 SVLoginExperienceDefinition 读取） */
	TSoftClassPtr<UCommonActivatableWidget> PressStartScreenClass;

	/** 主界面类（从 SVBaseExperienceDefinition 读取） */
	TSoftClassPtr<UCommonActivatableWidget> MainScreenClass;
};

#undef UE_API
