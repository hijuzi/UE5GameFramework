// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingProgressUserWidget.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

ULoadingProgressUserWidget::ULoadingProgressUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 默认背景：深色半透明
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Box;
	BackgroundBrush.TintColor = FLinearColor(0.05f, 0.05f, 0.1f, 0.9f);
}

void ULoadingProgressUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 初始化控件状态
	if (ProgressBar.IsValid())
	{
		ProgressBar->SetPercent(CurrentProgress);
	}
}

TSharedRef<SWidget> ULoadingProgressUserWidget::RebuildWidget()
{
	RootBorder = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::Black)
		.Padding(0);

	RootBorder->SetContent(
		SNew(SOverlay)
		// 背景图
		+ SOverlay::Slot()
		[
			SAssignNew(BackgroundImage, SImage)
			.Image(&BackgroundBrush)
		]
		// 进度控件层（居中）
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			// 进度文字
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0, 0, 0, 20)
			[
				SAssignNew(ProgressTextBlock, STextBlock)
				.Text(NSLOCTEXT("LoadingProgress", "DefaultText", "Loading..."))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 24))
				.ColorAndOpacity(FLinearColor::White)
			]
			// 进度条
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(200, 0)
			[
				SAssignNew(ProgressBar, SProgressBar)
				.Percent(CurrentProgress)
			]
		]
	);

	return RootBorder.ToSharedRef();
}

void ULoadingProgressUserWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	RootBorder.Reset();
	ProgressBar.Reset();
	ProgressTextBlock.Reset();
	BackgroundImage.Reset();
}

void ULoadingProgressUserWidget::SetProgress(float InProgress)
{
	CurrentProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	if (ProgressBar.IsValid())
	{
		ProgressBar->SetPercent(CurrentProgress);
	}
}

float ULoadingProgressUserWidget::GetProgress() const
{
	return CurrentProgress;
}

void ULoadingProgressUserWidget::SetProgressText(const FText& InText)
{
	if (ProgressTextBlock.IsValid())
	{
		ProgressTextBlock->SetText(InText);
	}
}

FText ULoadingProgressUserWidget::GetProgressText() const
{
	if (ProgressTextBlock.IsValid())
	{
		return ProgressTextBlock->GetText();
	}
	return FText::GetEmpty();
}

void ULoadingProgressUserWidget::SetBackgroundBrush(const FSlateBrush& InBrush)
{
	BackgroundBrush = InBrush;
	if (BackgroundImage.IsValid())
	{
		BackgroundImage->SetImage(&BackgroundBrush);
	}
}
