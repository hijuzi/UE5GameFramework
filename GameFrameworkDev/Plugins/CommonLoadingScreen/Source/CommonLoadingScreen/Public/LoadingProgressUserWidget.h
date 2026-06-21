// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "LoadingProgressUserWidget.generated.h"

class SBorder;
class SProgressBar;
class STextBlock;
class SImage;

/**
 * 加载进度界面控件。
 *
 * 核心结构：
 *   SBorder（根容器，撑满全屏）
 *     └── SOverlay
 *           ├── SImage（背景图）
 *           └── SVerticalBox（居中）
 *                 ├── STextBlock（进度文字）
 *                 └── SProgressBar（进度条）
 *
 * 与 BlackScreenUserWidget 不同，本控件没有淡入淡出动画，
 * 显示即完全可见，隐藏即完全不可见。
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

protected:
	//~ UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeConstruct() override;

private:
	/** 根 SBorder */
	TSharedPtr<SBorder> RootBorder;

	/** 进度条控件 */
	TSharedPtr<SProgressBar> ProgressBar;

	/** 进度文字控件 */
	TSharedPtr<STextBlock> ProgressTextBlock;

	/** 背景图片控件 */
	TSharedPtr<SImage> BackgroundImage;

	/** 背景画刷，派生蓝图可在编辑器中设置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading Progress", meta = (AllowPrivateAccess = "true"))
	FSlateBrush BackgroundBrush;

	/** 当前进度（0.0 ~ 1.0） */
	float CurrentProgress = 0.0f;
};
