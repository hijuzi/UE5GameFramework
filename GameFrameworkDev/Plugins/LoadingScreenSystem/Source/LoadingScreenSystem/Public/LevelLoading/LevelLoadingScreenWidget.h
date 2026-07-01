// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LoadingScreenWidget.h"
#include "LoadingScreenSettings.h"

#include "LevelLoadingScreenWidget.generated.h"

class SOverlay;
class SCanvas;
class SImage;

/**
 * 关卡加载界面控件。
 *
 * 继承自 ULoadingScreenWidget，在父类动画框架基础之上增加：
 * - Slate 控件层级：进度层（背景图 + 蓝图子类内容）+ 视频/图标层
 * - 从 ULoadingScreenSettings / ILevelLoadingScreenInterface 解析配置
 * - 通过 ULevelLoadingManager 获取多阶段精确加载进度
 *
 * 控件层级：
 *   SOverlay（根节点）
 *     ├── SOverlay（进度层，全局展开）
 *     │     ├── SImage（背景图）
 *     │     └── WidgetTree 内容（蓝图子类在此添加进度条等控件）
 *     └── SCanvas（视频/图标层，全局展开）
 *           └── SImage（视频占位图或跳过图标）
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class LOADINGSCREENSYSTEM_API ULevelLoadingScreenWidget : public ULoadingScreenWidget
{
	GENERATED_BODY()

public:
	ULevelLoadingScreenWidget(const FObjectInitializer& ObjectInitializer);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	// ================================================================
	// Progress
	// ================================================================

	/** 设置进度（0.0 ~ 1.0），供蓝图子类刷新进度条 */
	UFUNCTION(BlueprintCallable, Category = "LevelLoading|Progress")
	void SetProgress(float InProgress);

	/** 获取当前进度 */
	UFUNCTION(BlueprintPure, Category = "LevelLoading|Progress")
	float GetProgress() const;

	// ================================================================
	// Content
	// ================================================================

	/** 设置背景画刷 */
	UFUNCTION(BlueprintCallable, Category = "LevelLoading|Content")
	void SetBackgroundBrush(const FSlateBrush& InBrush);

	/** 设置视频/图标画刷 */
	UFUNCTION(BlueprintCallable, Category = "LevelLoading|Content")
	void SetVideoImageBrush(const FSlateBrush& InBrush);

protected:
	//~ UUserWidget interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	//~ ULoadingScreenWidget interface
	virtual void TickAnimation(float InDeltaTime) override;

	/** Image 类型时从 ULevelLoadingManager 刷新进度 */
	void TickProgressUpdate(float InDeltaTime);

private:
	// ================================================================
	// Slate 控件
	// ================================================================

	TSharedPtr<SOverlay> RootOverlay;
	TSharedPtr<SOverlay> ProgressOverlay;
	TSharedPtr<SImage> BackgroundImageWidget;
	TSharedPtr<SCanvas> VideoCanvas;
	TSharedPtr<SImage> VideoImageWidget;

	// ================================================================
	// 画刷与内容
	// ================================================================

	/** 背景画刷，派生蓝图可在编辑器中设置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelLoading|Content", meta = (AllowPrivateAccess = "true"))
	FSlateBrush BackgroundBrush;

	/** 视频/图标画刷，派生蓝图可在编辑器中设置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelLoading|Content", meta = (AllowPrivateAccess = "true"))
	FSlateBrush VideoImageBrush;

	// ================================================================
	// 进度
	// ================================================================

	/** 当前进度（0.0 ~ 1.0） */
	float CurrentProgress = 0.0f;

	// ================================================================
	// 配置（从 ULoadingScreenSettings + ILevelLoadingScreenInterface 解析）
	// ================================================================

	/** 加载界面内容类型（图片/视频），蓝图可读写 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelLoading|Content", meta = (AllowPrivateAccess = "true"))
	ELoadingScreenContentType ContentType = ELoadingScreenContentType::Image;

	/** 图片背景资产路径 */
	FSoftObjectPath ImageBackgroundPath;

	/** 视频路径 */
	FString VideoPath;

	/** 关卡加载界面最小显示时长（秒） */
	float MinimumDisplayTimeSecs = 2.0f;

	/** 累计时间，用于时间平滑上限 */
	float SmoothedProgressTime = 0.0f;

	/** 加载是否已完成 */
	bool bLoadingCompleted = false;

	// ================================================================
	// 配置解析
	// ================================================================

	/** 解析加载界面配置：优先 Interface 覆盖，否则全局 Settings */
	void ResolveConfig();

	/** 根据 ContentType 显示/隐藏对应层 */
	void ApplyContentTypeVisibility();

	/** 加载背景图资源 */
	void LoadBackgroundImage();

	/** 视频类型时使用 MoviePlayer 播放视频 */
	void PlayLoadingVideo();

	/** 关闭关卡加载界面回调 */
	void OnCloseLoadingScreen();

    UFUNCTION()
    void OnLoadingMovieFinished();
};
