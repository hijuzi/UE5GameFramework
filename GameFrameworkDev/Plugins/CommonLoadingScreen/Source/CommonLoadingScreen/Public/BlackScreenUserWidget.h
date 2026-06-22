// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "BlackScreenUserWidget.generated.h"

class SBorder;

/**
 * 黑屏淡入淡出的缓动类型
 */
UENUM(BlueprintType)
enum class EBlackScreenFadeEasing : uint8
{
	None		UMETA(DisplayName = "无"),
	EaseIn		UMETA(DisplayName = "缓入"),
	EaseOut		UMETA(DisplayName = "缓出"),
};

/** 黑屏动画完成时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBlackScreenAnimationCompleted);

/**
 * 黑屏控件的基类。
 *
 * 核心结构：
 *   SBorder（黑色背景，撑满全屏）
 *     └── 子控件区域（蓝图派生时可在此添加 Logo、加载提示等）
 *
 * 自动管理 RenderOpacity 的缓动动画：
 *   - PlayLoadAnimation()   ：淡入显示黑屏，完成后触发 OnLoadAnimationCompleted
 *   - PlayUnloadAnimation() ：淡出隐藏黑屏，完成后触发 OnUnloadAnimationCompleted
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class COMMONLOADINGSCREEN_API UBlackScreenUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBlackScreenUserWidget(const FObjectInitializer& ObjectInitializer);

	/** 获取根 SBorder，供 C++ 或蓝图向其中添加子控件 */
	TSharedPtr<SBorder> GetRootBorder() const { return RootBorder; }

	//~ 动画
	/** 播放加载动画（淡入显示黑屏），蓝图可重写 */
	UFUNCTION(BlueprintNativeEvent, Category = "Black Screen|Animation")
	void PlayLoadAnimation();

	/** 播放卸载动画（淡出隐藏黑屏），蓝图可重写 */
	UFUNCTION(BlueprintNativeEvent, Category = "Black Screen|Animation")
	void PlayUnloadAnimation();

	/** 是否正在缓动（淡入或淡出）中 */
	UFUNCTION(BlueprintPure, Category = "Black Screen|Animation")
	bool IsFading() const;

	/** 判断是否需要显示加载界面，蓝图可重载 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Black Screen")
	bool ShouldShowLoadingScreen() const;

	/** 加载动画完成时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Black Screen|Animation")
	FOnBlackScreenAnimationCompleted OnLoadAnimationCompleted;

	/** 卸载动画完成时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Black Screen|Animation")
	FOnBlackScreenAnimationCompleted OnUnloadAnimationCompleted;

protected:
	//~ UUserWidget interface
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 加载动画完成时调用，蓝图可重写 */
	UFUNCTION(BlueprintNativeEvent, Category = "Black Screen|Animation")
	void OnLoadAnimationFinished();

	/** 卸载动画完成时调用，蓝图可重写 */
	UFUNCTION(BlueprintNativeEvent, Category = "Black Screen|Animation")
	void OnUnloadAnimationFinished();

private:
	/** 根 SBorder，黑色背景全屏覆盖 */
	TSharedPtr<SBorder> RootBorder;

	/** 缓存的动画配置，在 NativeConstruct 中从 UCommonLoadingScreenSettings 读取 */
	float FadeInDuration = 0.3f;
	float FadeOutDuration = 0.3f;
	EBlackScreenFadeEasing FadeEasing = EBlackScreenFadeEasing::None;

	enum class EFadeState : uint8 { None, FadingIn, FadingOut };
	EFadeState FadeState = EFadeState::None;
	float FadeElapsed = 0.0f;

	/** Tick 中驱动自身渐入渐出动画 */
	void TickSelfFade(float InDeltaTime);

	static float ApplyEasing(float Alpha, EBlackScreenFadeEasing Easing);
};
