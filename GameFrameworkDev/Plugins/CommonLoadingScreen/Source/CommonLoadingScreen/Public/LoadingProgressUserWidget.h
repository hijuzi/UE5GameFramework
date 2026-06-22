// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "LoadingProgressUserWidget.generated.h"

class SBorder;
class SCanvas;
class SOverlay;
class SProgressBar;
class STextBlock;
class SImage;
class SVerticalBox;

/**
 * 遮罩淡入淡出的缓动类型
 */
UENUM(BlueprintType)
enum class EMaskFadeEasing : uint8
{
	None		UMETA(DisplayName = "无"),
	EaseIn		UMETA(DisplayName = "缓入"),
	EaseOut		UMETA(DisplayName = "缓出"),
};

/** 加载界面动画完成时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadingScreenAnimationCompleted);

/**
 * 加载进度界面控件。
 *
 * 核心结构：
 *   SOverlay（根节点）
 *     ├── SImage（背景图）
 *     ├── SVerticalBox（居中，进度层）
 *     │     ├── STextBlock（进度文字）
 *     │     └── SProgressBar（进度条）
 *     └── SCanvas（全局展开，遮罩画板，带渐入渐出动画）
 *           └── SBorder（全局展开，黑屏）
 *
 * 遮罩层自动管理 RenderOpacity 的缓动动画：
 *   - PlayLoadAnimation()   ：遮罩淡出，显示加载内容，完成后触发 OnLoadAnimationCompleted
 *   - PlayUnloadAnimation() ：遮罩淡入，遮盖加载内容，完成后触发 OnUnloadAnimationCompleted
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class COMMONLOADINGSCREEN_API ULoadingProgressUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULoadingProgressUserWidget(const FObjectInitializer& ObjectInitializer);

	/** 设置进度（0.0 ~ 1.0） */
	UFUNCTION(BlueprintCallable, Category = "Loading Progress")
	void SetProgress(float InProgress);

	/** 获取当前进度 */
	UFUNCTION(BlueprintPure, Category = "Loading Progress")
	float GetProgress() const;

	/** 设置进度文字 */
	UFUNCTION(BlueprintCallable, Category = "Loading Progress")
	void SetProgressText(const FText& InText);

	/** 获取当前进度文字 */
	UFUNCTION(BlueprintPure, Category = "Loading Progress")
	FText GetProgressText() const;

	/** 设置背景画刷 */
	UFUNCTION(BlueprintCallable, Category = "Loading Progress")
	void SetBackgroundBrush(const FSlateBrush& InBrush);

	//~ 动画
	/** 播放加载动画（遮罩淡出，显示加载内容），蓝图可重写 */
	UFUNCTION(BlueprintNativeEvent, Category = "Loading Progress|Animation")
	void PlayLoadAnimation();

	/** 播放卸载动画（遮罩淡入，遮盖加载内容），蓝图可重写 */
	UFUNCTION(BlueprintNativeEvent, Category = "Loading Progress|Animation")
	void PlayUnloadAnimation();

	/** 是否正在缓动（淡入或淡出）中 */
	UFUNCTION(BlueprintPure, Category = "Loading Progress|Animation")
	bool IsFading() const;

	/** 加载动画完成时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Loading Progress|Animation")
	FOnLoadingScreenAnimationCompleted OnLoadAnimationCompleted;

	/** 卸载动画完成时广播 */
	UPROPERTY(BlueprintAssignable, Category = "Loading Progress|Animation")
	FOnLoadingScreenAnimationCompleted OnUnloadAnimationCompleted;

protected:
	//~ UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 卸载动画完成时调用，蓝图可重写 */
	UFUNCTION(BlueprintNativeEvent, Category = "Loading Progress|Animation")
	void OnUnloadAnimationFinished();

	/** 加载动画完成时调用，蓝图可重写 */
	UFUNCTION(BlueprintNativeEvent, Category = "Loading Progress|Animation")
	void OnLoadAnimationFinished();

private:
	/** 根 SOverlay */
	TSharedPtr<SOverlay> RootOverlay;

	/** 进度条控件 */
	TSharedPtr<SProgressBar> ProgressBar;

	/** 进度文字控件 */
	TSharedPtr<STextBlock> ProgressTextBlock;

	/** 进度层容器（居中） */
	TSharedPtr<SVerticalBox> ProgressLayer;

	/** 背景图片控件 */
	TSharedPtr<SImage> BackgroundImage;

	/** 遮罩画板（全局展开） */
	TSharedPtr<SCanvas> MaskCanvas;

	/** 遮罩黑屏 */
	TSharedPtr<SBorder> MaskBorder;

	/** 背景画刷，派生蓝图可在编辑器中设置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading Progress", meta = (AllowPrivateAccess = "true"))
	FSlateBrush BackgroundBrush;

	/** 当前进度（0.0 ~ 1.0） */
	float CurrentProgress = 0.0f;

	/** 缓存的动画配置，在 NativeConstruct 中从 UCommonLoadingScreenSettings 读取 */
	float MaskFadeInDuration = 0.2f;
	float MaskFadeOutDuration = 0.2f;
	EMaskFadeEasing MaskFadeEasing = EMaskFadeEasing::None;

	enum class EMaskFadeState : uint8 { None, FadingIn, FadingOut };
	EMaskFadeState MaskFadeState = EMaskFadeState::None;
	float MaskFadeElapsed = 0.0f;

	/** Tick 中驱动遮罩渐入渐出动画 */
	void TickMaskFade(float InDeltaTime);

	static float ApplyEasing(float Alpha, EMaskFadeEasing Easing);
};
