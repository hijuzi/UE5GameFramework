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
	FadeInDuration = Settings->BlackScreenFadeInDuration;
	FadeOutDuration = Settings->BlackScreenFadeOutDuration;
}

TSharedRef<SWidget> UBlackScreenUserWidget::RebuildWidget()
{
	RootBorder = SNew(SBorder)
		.BorderBackgroundColor(FLinearColor::White)
		.Padding(0);

	// 初始状态：完全透明，等待 PlayFadeIn 调用
	SetRenderOpacity(0.0f);

	// 允许 Blueprint 派生的 WidgetTree 作为 SBorder 的子内容嵌入
	if (WidgetTree)
	{
		TSharedRef<SWidget> Content = Super::RebuildWidget();
		RootBorder->SetContent(Content);
	}

	return RootBorder.ToSharedRef();
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

void UBlackScreenUserWidget::PlayFadeIn()
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

void UBlackScreenUserWidget::PlayFadeOut()
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

void UBlackScreenUserWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsFading())
	{
		return;
	}

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

		if (CompletedState == EFadeState::FadingIn)
		{
			SetRenderOpacity(1.0f);
			OnFadeInCompleted.Broadcast();
		}
		else
		{
			SetRenderOpacity(0.0f);
			OnFadeOutCompleted.Broadcast();
		}
	}
}

float UBlackScreenUserWidget::ApplyEasing(float Alpha, EBlackScreenFadeEasing Easing)
{
	switch (Easing)
	{
	case EBlackScreenFadeEasing::None:
		return Alpha;
	case EBlackScreenFadeEasing::EaseIn:
		return Alpha * Alpha;
	case EBlackScreenFadeEasing::EaseOut:
		return Alpha * (2.0f - Alpha);
	default:
		return Alpha;
	}
}
