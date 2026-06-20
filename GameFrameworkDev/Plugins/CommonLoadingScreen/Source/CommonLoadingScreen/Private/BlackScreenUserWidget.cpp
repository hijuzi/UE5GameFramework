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
	FadeEasing = Settings->BlackScreenFadeEasing;
}

TSharedRef<SWidget> UBlackScreenUserWidget::RebuildWidget()
{
	RootBorder = SNew(SBorder)
		.BorderBackgroundColor(FLinearColor::Black)
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
	// 不重置 RenderOpacity —— 保留当前值作为起点，避免跳变
}

void UBlackScreenUserWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (FadeState == EFadeState::None)
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
	case EBlackScreenFadeEasing::Linear:
		return Alpha;
	case EBlackScreenFadeEasing::EaseIn:
		return Alpha * Alpha;
	case EBlackScreenFadeEasing::EaseOut:
		return Alpha * (2.0f - Alpha);
	case EBlackScreenFadeEasing::EaseInOut:
		return Alpha < 0.5f
			? 2.0f * Alpha * Alpha
			: 1.0f - FMath::Pow(-2.0f * Alpha + 2.0f, 2.0f) * 0.5f;
	default:
		return Alpha;
	}
}
