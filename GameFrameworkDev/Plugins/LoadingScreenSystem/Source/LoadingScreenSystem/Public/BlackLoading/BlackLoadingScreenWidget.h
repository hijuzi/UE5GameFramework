// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LoadingScreenWidget.h"
#include "BlackLoadingScreenWidget.generated.h"

class SBorder;
class SOverlay;

/**
 * 黑屏加载控件，基于 Slate 的黑屏遮罩 + 蓝图内容层。
 *
 * 核心 Slate 结构：
 *   SOverlay（根节点）
 *     └── SOverlay（遮罩画板，全局展开，带渐入渐出动画）
 *           └── SBorder（全局展开，黑屏填充）
 *     └── SOverlay（内容层，全局展开）
 *           └── WidgetTree 内容（蓝图子类在此添加自定义控件）
 *
 * 动画流程：
 *   - StartLoadAnimation()  ：遮罩从透明淡入到不透明（显示黑屏）
 *   - StartUnloadAnimation()：遮罩从不透明淡出到透明（隐藏黑屏）
 *   动画完成后分别触发 OnLoadAnimationCompleted / OnUnloadAnimationCompleted
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class LOADINGSCREENSYSTEM_API UBlackLoadingScreenWidget : public ULoadingScreenWidget
{
	GENERATED_BODY()

public:
	UBlackLoadingScreenWidget(const FObjectInitializer& ObjectInitializer);

	/** 获取内容层 SOverlay，供 C++ 或蓝图向其中添加子控件 */
	TSharedPtr<SOverlay> GetContentOverlay() const { return ContentOverlay; }

protected:
	//~ UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	//~ ULoadingScreenWidget interface
	virtual void StartLoadAnimation_Implementation() override;
	virtual void StartUnloadAnimation_Implementation() override;
	virtual void TickAnimation(float InDeltaTime) override;
	virtual void FinishLoadAnimation_Implementation() override;
	virtual void FinishUnloadAnimation_Implementation() override;

private:
	/** 根 SOverlay */
	TSharedPtr<SOverlay> RootOverlay;

	/** 遮罩层（全局展开），带渐入渐出动画 */
	TSharedPtr<SOverlay> MaskOverlay;

	/** 遮罩黑屏边框 */
	TSharedPtr<SBorder> MaskBorder;

	/** 内容层（全局展开），承载 WidgetTree 蓝图内容 */
	TSharedPtr<SOverlay> ContentOverlay;

	float ApplyEasing(float Alpha, ELoadingScreenAnimationState State);
};
