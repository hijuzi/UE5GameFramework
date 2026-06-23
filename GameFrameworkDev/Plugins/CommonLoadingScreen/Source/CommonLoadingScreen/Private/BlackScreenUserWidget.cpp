// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackScreenUserWidget.h"
#include "CommonLoadingScreenSettings.h"

#include "Widgets/Layout/SBorder.h"

UBlackScreenUserWidget::UBlackScreenUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBlackScreenUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 从项目设置中读取动画配置，避免外部注入
	const UCommonLoadingScreenSettings* Settings = GetDefault<UCommonLoadingScreenSettings>();
	FadeInDuration = Settings->BlackScreenLoadDuration;
	FadeOutDuration = Settings->BlackScreenUnloadDuration;

	// Widget 显示即启动 Ticker（暂停时也不中断）
	StartTicker();
}

TSharedRef<SWidget> UBlackScreenUserWidget::RebuildWidget()
{
	RootBorder = SNew(SBorder)
		.BorderBackgroundColor(FLinearColor::Black)
		.Padding(0);

	// 初始状态：完全透明，等待 PlayLoadAnimation 调用
	SetRenderOpacity(0.0f);

	// 允许 Blueprint 派生的 WidgetTree 作为 SBorder 的子内容嵌入
	if (WidgetTree)
	{
		TSharedRef<SWidget> Content = Super::RebuildWidget();
		RootBorder->SetContent(Content);
	}

	return RootBorder.ToSharedRef();
}

void UBlackScreenUserWidget::NativeDestruct()
{
	StopTicker();
	Super::NativeDestruct();
}

void UBlackScreenUserWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	RootBorder.Reset();
}

bool UBlackScreenUserWidget::IsFading() const
{
	return FadeState != EFadeState::None;
}

bool UBlackScreenUserWidget::ShouldShowLoadingScreen_Implementation() const
{
	return true;
}

void UBlackScreenUserWidget::PlayLoadAnimation_Implementation()
{
	if (FadeState == EFadeState::FadingIn)
	{
		return; // 已在淡入中
	}

	FadeState = EFadeState::FadingIn;
	FadeElapsed = 0.0f;
	SetRenderOpacity(0.0f);
	SetVisibility(ESlateVisibility::Visible);
}

void UBlackScreenUserWidget::PlayUnloadAnimation_Implementation()
{
	if (FadeState == EFadeState::FadingOut)
	{
		return; // 已在淡出中
	}

	FadeState = EFadeState::FadingOut;
	FadeElapsed = 0.0f;
	SetRenderOpacity(1.0f);
	SetVisibility(ESlateVisibility::Visible);
}

void UBlackScreenUserWidget::TickSelfFade(float InDeltaTime)
{
	const float Duration = (FadeState == EFadeState::FadingIn) ? FadeInDuration : FadeOutDuration;
	FadeElapsed += InDeltaTime;

	if (Duration > 0.0f)
	{
		float Alpha = FMath::Clamp(FadeElapsed / Duration, 0.0f, 1.0f);
		Alpha = ApplyEasing(Alpha, FadeEasing);

		SetRenderOpacity((FadeState == EFadeState::FadingIn) ? Alpha : (1.0f - Alpha));
	}

	if (FadeElapsed >= Duration)
	{
		const EFadeState CompletedState = FadeState;
		FadeState = EFadeState::None;

		// Ticker 保持运行（Widget 仍显示中），只是停止动画驱动

		if (CompletedState == EFadeState::FadingIn)
		{
			OnLoadAnimationFinished();
		}
		else
		{
			OnUnloadAnimationFinished();
		}
	}
}

void UBlackScreenUserWidget::CustomTick(float InDeltaTime)
{
	if (IsFading())
	{
		TickSelfFade(InDeltaTime);
	}
}

void UBlackScreenUserWidget::StartTicker()
{
	if (!TickerHandle.IsValid())
	{
		TWeakObjectPtr<UBlackScreenUserWidget> WeakThis(this);
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis](float DeltaTime) -> bool
			{
				UBlackScreenUserWidget* StrongThis = WeakThis.Get();
				if (!StrongThis)
				{
					return false;
				}
				StrongThis->CustomTick(DeltaTime);
				return true;
			}));
	}
}

void UBlackScreenUserWidget::StopTicker()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

float UBlackScreenUserWidget::ApplyEasing(float Alpha, EFadeEasing Easing)
{
	switch (Easing)
	{
	case EFadeEasing::None:
		return Alpha;
	case EFadeEasing::EaseIn:
		return Alpha * Alpha;
	case EFadeEasing::EaseOut:
		return Alpha * (2.0f - Alpha);
	default:
		return Alpha;
	}
}

void UBlackScreenUserWidget::OnLoadAnimationFinished_Implementation()
{
	SetRenderOpacity(1.0f);
	OnLoadAnimationCompleted.Broadcast();
}

void UBlackScreenUserWidget::OnUnloadAnimationFinished_Implementation()
{
	SetRenderOpacity(0.0f);
	OnUnloadAnimationCompleted.Broadcast();
}
