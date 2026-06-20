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
	Linear		UMETA(DisplayName = "线性"),
	EaseIn		UMETA(DisplayName = "缓入"),
	EaseOut		UMETA(DisplayName = "缓出"),
	EaseInOut	UMETA(DisplayName = "缓入缓出"),
};

/** 黑屏动画完成时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBlackScreenFadeCompleted);

/**
 * 黑屏控件的基类。
 *
 * 核心结构：
 *   SBorder（黑色背景，撑满全屏）
 *     └── 子控件区域（蓝图派生时可在此添加 Logo、加载提示等）
 *
 * 自动管理 RenderOpacity 的缓动动画：
 *   - PlayFadeIn() ：0 → 1，完成后触发 OnFadeInCompleted
 *   - PlayFadeOut()：1 → 0，完成后触发 OnFadeOutCompleted
 */
UCLASS(BlueprintType, Blueprintable)
class COMMONLOADINGSCREEN_API UBlackScreenUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBlackScreenUserWidget(const FObjectInitializer& ObjectInitializer);

	/** 获取根 SBorder，供 C++ 或蓝图向其中添加子控件 */
	TSharedPtr<SBorder> GetRootBorder() const { return RootBorder; }

	//~ 播放动画
	UFUNCTION(BlueprintCallable, Category = "Black Screen")
	void PlayFadeIn();

	UFUNCTION(BlueprintCallable, Category = "Black Screen")
	void PlayFadeOut();

	/** 淡入动画完成时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Black Screen")
	FOnBlackScreenFadeCompleted OnFadeInCompleted;

	/** 淡出动画完成时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Black Screen")
	FOnBlackScreenFadeCompleted OnFadeOutCompleted;

protected:
	//~ UUserWidget interface
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** 根 SBorder，黑色背景全屏覆盖 */
	TSharedPtr<SBorder> RootBorder;

	/** 缓存的动画配置，在 NativeConstruct 中从 UCommonLoadingScreenSettings 读取 */
	float FadeInDuration = 0.3f;
	float FadeOutDuration = 0.3f;
	EBlackScreenFadeEasing FadeEasing = EBlackScreenFadeEasing::EaseInOut;

	enum class EFadeState : uint8 { None, FadingIn, FadingOut };
	EFadeState FadeState = EFadeState::None;
	float FadeElapsed = 0.0f;

	static float ApplyEasing(float Alpha, EBlackScreenFadeEasing Easing);
};
