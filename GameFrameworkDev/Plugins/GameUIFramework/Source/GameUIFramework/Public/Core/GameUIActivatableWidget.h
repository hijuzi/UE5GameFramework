// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"

#include "GameUIActivatableWidget.generated.h"

struct FUIInputConfig;

/**
 * EGameUIWidgetInputMode
 *
 * 决定了 ActivatableWidget 激活时
 * 驱动 UCommonActivatableWidget 使用何种 FUIInputConfig（输入模式 + 鼠标捕获策略）。
 *
 * 每个枚举值对应 GetDesiredInputConfig() 中的一个分支，直接映射到
 * ECommonInputMode（输入路由）和 EMouseCaptureMode（鼠标行为）。
 */
UENUM(BlueprintType)
enum class EGameUIWidgetInputMode : uint8
{
	/** 默认模式：不覆盖输入配置，沿用 CommonUI 或父级 Widget 的已有设置 */
	Default,
	/** 混合模式：同时接收 UI 输入和游戏输入（ECommonInputMode::All）；
	 *  鼠标捕获规则由 GameMouseCaptureMode 属性指定。适用于 HUD、聊天框等叠加层。 */
	GameAndMenu,
	/** 纯游戏模式：输入仅路由到游戏/PlayerController（ECommonInputMode::Game）；
	 *  鼠标捕获规则由 GameMouseCaptureMode 属性指定。适用于准星、操作提示等。 */
	Game,
	/** 纯菜单模式：输入仅路由到 UI（ECommonInputMode::Menu）；
	 *  鼠标无条件释放（EMouseCaptureMode::NoCapture）。适用于暂停菜单、设置面板等全屏 UI。 */
	Menu
};

/**
 * UGameUIActivatableWidget
 *
 * 可激活控件基类。
 * 在激活时自动驱动所需的 FUIInputConfig，免去每个子类手动覆写 GetDesiredInputConfig()。
 *
 * 子类只需在蓝图中设置 InputConfig（EGameUIWidgetInputMode），即可控制：
 *  - 输入路由到游戏还是 UI
 *  - 鼠标捕获/显示策略
 */
UCLASS(Abstract, Blueprintable)
class UGameUIActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UGameUIActivatableWidget(const FObjectInitializer& ObjectInitializer);
	
public:
	
	//~UCommonActivatableWidget interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End of UCommonActivatableWidget interface

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const override;
#endif
	
protected:
	/** 此 UI 激活时期望的输入模式，例如：按键事件是否仍需传递到游戏/玩家控制器？ */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EGameUIWidgetInputMode InputConfig = EGameUIWidgetInputMode::Default;

	/** 游戏获取输入时期望的鼠标行为。 */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;
};
