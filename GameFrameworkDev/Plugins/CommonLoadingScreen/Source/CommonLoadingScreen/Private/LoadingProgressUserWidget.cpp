// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingProgressUserWidget.h"
#include "CommonLoadingScreenSettings.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SCanvas.h"
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
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	BackgroundBrush.TintColor = FLinearColor::Black;
}

void ULoadingProgressUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 从项目设置中读取遮罩动画配置
	const UCommonLoadingScreenSettings* Settings = GetDefault<UCommonLoadingScreenSettings>();
	MaskFadeInDuration = Settings->MaskFadeInDuration;
	MaskFadeOutDuration = Settings->MaskFadeOutDuration;

	// 初始化控件状态
	if (ProgressBar.IsValid())
	{
		ProgressBar->SetPercent(CurrentProgress);
	}
}

TSharedRef<SWidget> ULoadingProgressUserWidget::RebuildWidget()
{
	RootOverlay = SNew(SOverlay)
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
			SAssignNew(ProgressLayer, SVerticalBox)
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
		// 遮罩画板（全局展开）
		+ SOverlay::Slot()
		[
			SAssignNew(MaskCanvas, SCanvas)
			+ SCanvas::Slot()
			.Position(FVector2D::ZeroVector)
			.Size(FVector2D(1.0f, 1.0f))
			[
				SAssignNew(MaskBorder, SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor::Black)
				.Padding(0)
			]
		];

	return RootOverlay.ToSharedRef();
}

void ULoadingProgressUserWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	RootOverlay.Reset();
	MaskBorder.Reset();
	MaskCanvas.Reset();
	ProgressBar.Reset();
	ProgressTextBlock.Reset();
	ProgressLayer.Reset();
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

void ULoadingProgressUserWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsFading())
	{
		return;
	}

	const float Duration = (MaskFadeState == EMaskFadeState::FadingIn) ? MaskFadeInDuration : MaskFadeOutDuration;
	MaskFadeElapsed += InDeltaTime;

	if (Duration > 0.0f)
	{
		float Alpha = FMath::Clamp(MaskFadeElapsed / Duration, 0.0f, 1.0f);
		Alpha = ApplyEasing(Alpha, MaskFadeEasing);

		if (MaskCanvas.IsValid())
		{
			MaskCanvas->SetRenderOpacity((MaskFadeState == EMaskFadeState::FadingIn) ? Alpha : (1.0f - Alpha));
		}
	}

	if (MaskFadeElapsed >= Duration)
	{
		const EMaskFadeState CompletedState = MaskFadeState;
		MaskFadeState = EMaskFadeState::None;

	if (CompletedState == EMaskFadeState::FadingIn)
	{
		OnMaskFadeInFinished();
	}
	else
	{
		OnMaskFadeOutFinished();
	}
	}
}

void ULoadingProgressUserWidget::PlayMaskFadeIn()
{
	if (MaskFadeState == EMaskFadeState::FadingIn)
	{
		return;
	}

	MaskFadeState = EMaskFadeState::FadingIn;
	MaskFadeElapsed = 0.0f;
	if (MaskCanvas.IsValid())
	{
		MaskCanvas->SetRenderOpacity(0.0f);
	}
}

void ULoadingProgressUserWidget::PlayMaskFadeOut()
{
	if (MaskFadeState == EMaskFadeState::FadingOut)
	{
		return;
	}

	MaskFadeState = EMaskFadeState::FadingOut;
	MaskFadeElapsed = 0.0f;
	if (MaskCanvas.IsValid())
	{
		MaskCanvas->SetRenderOpacity(1.0f);
	}
}

bool ULoadingProgressUserWidget::IsFading() const
{
	return MaskFadeState != EMaskFadeState::None;
}

float ULoadingProgressUserWidget::ApplyEasing(float Alpha, EMaskFadeEasing Easing)
{
	switch (Easing)
	{
	case EMaskFadeEasing::None:
		return Alpha;
	case EMaskFadeEasing::EaseIn:
		return Alpha * Alpha;
	case EMaskFadeEasing::EaseOut:
		return Alpha * (2.0f - Alpha);
	default:
		return Alpha;
	}
}

void ULoadingProgressUserWidget::OnMaskFadeInFinished_Implementation()
{
	if (MaskCanvas.IsValid())
	{
		MaskCanvas->SetRenderOpacity(1.0f);
	}
	OnMaskFadeInCompleted.Broadcast();
}

void ULoadingProgressUserWidget::OnMaskFadeOutFinished_Implementation()
{
	if (MaskCanvas.IsValid())
	{
		MaskCanvas->SetRenderOpacity(0.0f);
	}
	OnMaskFadeOutCompleted.Broadcast();
}
