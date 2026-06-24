// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackScreenUserWidget.h"
#include "CommonLoadingScreenSettings.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Styling/CoreStyle.h"

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
	RootOverlay = SNew(SOverlay)
		// --- 遮罩层：黑屏，带渐入渐出动画（Slot 0，下层） ---
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(MaskOverlay, SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(MaskBorder, SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor::Black)
				.Padding(0)
			]
		]
		// --- 内容层：蓝图派生内容（Slot 1，上层） ---
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(ContentOverlay, SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				WidgetTree ? Super::RebuildWidget() : SNullWidget::NullWidget
			]
		];

	return RootOverlay.ToSharedRef();
}

void UBlackScreenUserWidget::NativeDestruct()
{
	StopTicker();
	Super::NativeDestruct();
}

void UBlackScreenUserWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	RootOverlay.Reset();
	MaskOverlay.Reset();
	MaskBorder.Reset();
	ContentOverlay.Reset();
}

bool UBlackScreenUserWidget::IsFading() const
{
	return FadeEasing != EFadeEasing::None;
}

bool UBlackScreenUserWidget::ShouldShowLoadingScreen_Implementation() const
{
	return true;
}

void UBlackScreenUserWidget::PlayLoadAnimation_Implementation()
{
	if (FadeEasing == EFadeEasing::EaseIn)
	{
		return; // 已在淡入中
	}

	FadeEasing = EFadeEasing::EaseIn;
	FadeElapsed = 0.0f;
	MaskOverlay->SetRenderOpacity(0.0f);
	SetVisibility(ESlateVisibility::Visible);
}

void UBlackScreenUserWidget::PlayUnloadAnimation_Implementation()
{
	if (FadeEasing == EFadeEasing::EaseOut)
	{
		return; // 已在淡出中
	}

	FadeEasing = EFadeEasing::EaseOut;
	FadeElapsed = 0.0f;
	MaskOverlay->SetRenderOpacity(1.0f);
	SetVisibility(ESlateVisibility::Visible);
}

void UBlackScreenUserWidget::TickSelfFade(float InDeltaTime)
{
	const float Duration = (FadeEasing == EFadeEasing::EaseIn) ? FadeInDuration : FadeOutDuration;
	FadeElapsed += InDeltaTime;

	if (Duration > 0.0f)
	{
		float Alpha = FMath::Clamp(FadeElapsed / Duration, 0.0f, 1.0f);
		Alpha = ApplyEasing(Alpha, FadeEasing);

		const float Opacity = (FadeEasing == EFadeEasing::EaseIn) ? Alpha : (1.0f - Alpha);
		MaskOverlay->SetRenderOpacity(Opacity);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("[BlackScreen] Opacity: %.3f | Easing: %s"), Opacity, FadeEasing == EFadeEasing::EaseIn ? TEXT("EaseIn") : TEXT("EaseOut")));
		}
	}

	if (FadeElapsed >= Duration)
	{
		const EFadeEasing CompletedState = FadeEasing;
		FadeEasing = EFadeEasing::None;

		// Ticker 保持运行（Widget 仍显示中），只是停止动画驱动

		if (CompletedState == EFadeEasing::EaseIn)
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
		return Alpha;
	case EFadeEasing::EaseOut:
		return Alpha;
	default:
		return Alpha;
	}
}

void UBlackScreenUserWidget::OnLoadAnimationFinished_Implementation()
{
	MaskOverlay->SetRenderOpacity(1.0f);
	OnLoadAnimationCompleted.Broadcast();
}

void UBlackScreenUserWidget::OnUnloadAnimationFinished_Implementation()
{
	MaskOverlay->SetRenderOpacity(0.0f);
	OnUnloadAnimationCompleted.Broadcast();
}
