// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CommonLoadingScreenSettings.h"
#include "Containers/Ticker.h"

#include "LoadingProgressUserWidget.generated.h"

class SBorder;
class SCanvas;
class SOverlay;
class SImage;

/** 加载界面动画完成时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadingScreenAnimationCompleted);

/**
 * 加载进度界面控件。
 *
 * 核心结构：
 *   SOverlay（根节点）
 *     ├── SOverlay（进度层，全局展开）
 *     │     ├── SImage（背景图）
 *     │     └── WidgetTree 内容（蓝图子类在此添加自定义控件）
 *     │
 *     ├── SCanvas（视频层，全局展开）
 *     │
 *     └── SOverlay（全局展开，遮罩画板，带渐入渐出动画）
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
	
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/** 设置进度（0.0 ~ 1.0），BlueprintNativeEvent 供蓝图子类重载刷新逻辑 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Loading Progress")
	void SetProgress(float InProgress);

	/** 获取当前进度 */
	UFUNCTION(BlueprintPure, Category = "Loading Progress")
	float GetProgress() const;

	/** 设置背景画刷 */
	UFUNCTION(BlueprintCallable, Category = "Loading Progress")
	void SetBackgroundBrush(const FSlateBrush& InBrush);

	/** 设置视频/图标画刷 */
	UFUNCTION(BlueprintCallable, Category = "Loading Progress")
	void SetVideoImageBrush(const FSlateBrush& InBrush);

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
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** 卸载动画完成时调用，蓝图可重写 */
	UFUNCTION(BlueprintNativeEvent, Category = "Loading Progress|Animation")
	void OnUnloadAnimationFinished();

	/** 加载动画完成时调用，蓝图可重写 */
	UFUNCTION(BlueprintNativeEvent, Category = "Loading Progress|Animation")
	void OnLoadAnimationFinished();

private:
	/** 根 SOverlay */
	TSharedPtr<SOverlay> RootOverlay;

	/** 进度层（全局展开），包含背景图和蓝图内容 */
	TSharedPtr<SOverlay> ProgressOverlay;

	/** 背景图片控件 */
	TSharedPtr<SImage> BackgroundImage;

	/** 视频层（全局展开），蓝图可访问设置具体内容 */
	TSharedPtr<SCanvas> VideoCanvas;

	/** 视频/图标控件（跳过图标等），蓝图可访问 */
	TSharedPtr<SImage> VideoImage;

	/** 遮罩层（全局展开），带渐入渐出动画 */
	TSharedPtr<SOverlay> MaskOverlay;

	/** 遮罩黑屏 */
	TSharedPtr<SBorder> MaskBorder;

	/** 背景画刷，派生蓝图可在编辑器中设置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading Progress", meta = (AllowPrivateAccess = "true"))
	FSlateBrush BackgroundBrush;

	/** 视频/图标画刷，派生蓝图可在编辑器中设置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading Progress", meta = (AllowPrivateAccess = "true"))
	FSlateBrush VideoImageBrush;

	/** 当前进度（0.0 ~ 1.0） */
	float CurrentProgress = 0.0f;

	//~ 缓存的动画/内容配置，NativeConstruct 中解析（优先 Experience 覆盖，否则全局 Settings）

	/** 加载动画时长 */
	float LoadingScreenLoadDuration = 0.2f;
	float LoadingScreenUnloadDuration = 0.2f;

	/** 加载界面动画过渡类型 */
	ECommonLoadingAnimationType LoadingScreenAnimationType = ECommonLoadingAnimationType::Opacity;

	/** 加载界面动画插值模式 */
	ECommonLoadingAnimationMode LoadingScreenAnimationMode = ECommonLoadingAnimationMode::Linear;

	/** 加载界面内容类型（图片/视频），蓝图可读写 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading Progress", meta = (AllowPrivateAccess = "true"))
	ECommonLoadingScreenContentType LoadingScreenContentType = ECommonLoadingScreenContentType::Image;

	/** 图片背景路径 */
	FSoftObjectPath LoadingScreenImageBackground;

	/** 视频路径 */
	FString LoadingScreenVideoPath;

	/** 加载界面最小显示时长（秒） */
	float MinimumLoadingScreenDisplayTimeSecs = 2.0f;

	/** 平滑进度累计时间，用于限制进度增速不超过 MinimumLoadingScreenDisplayTime 的线性进度 */
	float SmoothedProgressTime = 0.0f;

	/** 标记加载是否已完成 */
	bool bLoadingCompleted = false;

	//~ 配置解析

	/**
	 * 解析加载界面配置：
	 * - 优先从当前 Experience（USVBaseExperienceDefinition）读取覆盖参数
	 * - 否则从 UCommonLoadingScreenSettings 全局设置读取
	 */
	void ResolveLoadingScreenConfig();

	/** 加载动画准备：设置遮罩层为不透明 */
	void PrepareLoadAnimation();

	/** 卸载动画准备：重置计时器，保持遮罩当前状态 */
	void PrepareUnloadAnimation();

	/** 根据 LoadingScreenContentType 显示/隐藏 ProgressOverlay 与 VideoCanvas */
	void ApplyContentTypeVisibility();

	/** 从 LoadingScreenImageBackground 加载并设置背景图 */
	void LoadBackgroundImage();

	/** 视频类型时，使用引擎内置 MoviePlayer 播放 LoadingScreenVideoPath */
	void PlayLoadingVideo();

	/** 视频播放完成后回调，触发隐藏加载界面 */
	void OnLoadingMovieFinished();

	EFadeEasing MaskFadeState = EFadeEasing::None;
	float MaskFadeElapsed = 0.0f;

	/** 替代 NativeTick，由 FTSTicker 驱动（暂停时仍运行） */
	void CustomTick(float InDeltaTime);

	/** Tick 中驱动遮罩渐入渐出动画 */
	void TickMaskFade(float InDeltaTime);

	/** 从子系统更新进度并刷新 UI */
	void TickProgressUpdate(float InDeltaTime);

	static float ApplyEasing(float Alpha, EFadeEasing Easing);

	FTSTicker::FDelegateHandle TickerHandle;
	void StartTicker();
	void StopTicker();
};
