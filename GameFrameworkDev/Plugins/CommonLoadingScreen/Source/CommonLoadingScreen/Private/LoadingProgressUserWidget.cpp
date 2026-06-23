// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingProgressUserWidget.h"
#include "CommonLoadingScreenSettings.h"
#include "LevelLoadingProgressSubsystem.h"
#include "LoadingScreenManager.h"

#include "Engine/GameInstance.h"
#include "Containers/Ticker.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Styling/CoreStyle.h"

ULoadingProgressUserWidget::ULoadingProgressUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 默认背景：深色半透明
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	BackgroundBrush.TintColor = FLinearColor::Black;
}

void ULoadingProgressUserWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 预览时遮罩层透明，方便蓝图编辑进度层中的子控件
	if (MaskCanvas.IsValid())
	{
		MaskCanvas->SetRenderOpacity(0.0f);
	}
}

void ULoadingProgressUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 确保遮罩层运行时为不透明（预览时 NativePreConstruct 将其设为透明）
	if (MaskCanvas.IsValid())
	{
		MaskCanvas->SetRenderOpacity(1.0f);
	}

	// 从项目设置中读取遮罩动画配置
	const UCommonLoadingScreenSettings* Settings = GetDefault<UCommonLoadingScreenSettings>();
	MaskFadeInDuration = Settings->MaskFadeInDuration;
	MaskFadeOutDuration = Settings->MaskFadeOutDuration;

	// 启动独立于世界暂停的 Ticker，驱动进度更新和遮罩动画
	StartTicker();
}

TSharedRef<SWidget> ULoadingProgressUserWidget::RebuildWidget()
{
	RootOverlay = SNew(SOverlay)
		// 背景图
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(BackgroundImage, SImage)
			.Image(&BackgroundBrush)
		]
		// 蓝图派生内容（进度控件等）
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			WidgetTree ? Super::RebuildWidget() : SNullWidget::NullWidget
		]
		// 遮罩画板（全局展开）
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
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

void ULoadingProgressUserWidget::NativeDestruct()
{
	StopTicker();
	Super::NativeDestruct();
}

void ULoadingProgressUserWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	RootOverlay.Reset();
	MaskBorder.Reset();
	MaskCanvas.Reset();
	BackgroundImage.Reset();
}

void ULoadingProgressUserWidget::SetProgress_Implementation(float InProgress)
{
	CurrentProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
}

float ULoadingProgressUserWidget::GetProgress() const
{
	return CurrentProgress;
}

void ULoadingProgressUserWidget::SetBackgroundBrush(const FSlateBrush& InBrush)
{
	BackgroundBrush = InBrush;
	if (BackgroundImage.IsValid())
	{
		BackgroundImage->SetImage(&BackgroundBrush);
	}
}

void ULoadingProgressUserWidget::TickProgressUpdate()
{
	// 从子系统获取加载进度
	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULevelLoadingProgressSubsystem* Subsys = GI->GetSubsystem<ULevelLoadingProgressSubsystem>())
		{
			const float RawProgress = Subsys->GetPreciseLoadingProgress();
			SetProgress(RawProgress / 100.0f);

			if (RawProgress >= 100.0f)
			{
				if (ULoadingScreenManager* Manager = GI->GetSubsystem<ULoadingScreenManager>())
				{
					Manager->PrepareHideLoadingScreen();
				}
			}
		}
	}
}

void ULoadingProgressUserWidget::CustomTick(float InDeltaTime)
{
	TickProgressUpdate();

	if (IsFading())
	{
		TickMaskFade(InDeltaTime);
	}
}

void ULoadingProgressUserWidget::StartTicker()
{
	if (!TickerHandle.IsValid())
	{
		TWeakObjectPtr<ULoadingProgressUserWidget> WeakThis(this);
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis](float DeltaTime) -> bool
			{
				ULoadingProgressUserWidget* StrongThis = WeakThis.Get();
				if (!StrongThis)
				{
					return false;
				}
				StrongThis->CustomTick(DeltaTime);
				return true;
			}));
	}
}

void ULoadingProgressUserWidget::StopTicker()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

void ULoadingProgressUserWidget::TickMaskFade(float InDeltaTime)
{
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
			OnUnloadAnimationFinished();
		}
		else
		{
			OnLoadAnimationFinished();
		}
	}
}

void ULoadingProgressUserWidget::PlayUnloadAnimation_Implementation()
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

void ULoadingProgressUserWidget::PlayLoadAnimation_Implementation()
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

void ULoadingProgressUserWidget::OnUnloadAnimationFinished_Implementation()
{
	if (MaskCanvas.IsValid())
	{
		MaskCanvas->SetRenderOpacity(1.0f);
	}
	OnUnloadAnimationCompleted.Broadcast();
}

void ULoadingProgressUserWidget::OnLoadAnimationFinished_Implementation()
{
	if (MaskCanvas.IsValid())
	{
		MaskCanvas->SetRenderOpacity(0.0f);
	}
	OnLoadAnimationCompleted.Broadcast();
}
