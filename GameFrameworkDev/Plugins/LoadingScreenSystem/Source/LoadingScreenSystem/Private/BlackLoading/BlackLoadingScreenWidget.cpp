// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackLoading/BlackLoadingScreenWidget.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Styling/CoreStyle.h"

UBlackLoadingScreenWidget::UBlackLoadingScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBlackLoadingScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (MaskOverlay.IsValid())
	{
		MaskOverlay->SetRenderOpacity(0.0f);
	}
}

TSharedRef<SWidget> UBlackLoadingScreenWidget::RebuildWidget()
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

void UBlackLoadingScreenWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UBlackLoadingScreenWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	RootOverlay.Reset();
	MaskOverlay.Reset();
	MaskBorder.Reset();
	ContentOverlay.Reset();
}

void UBlackLoadingScreenWidget::StartLoadAnimation_Implementation()
{
    Super::StartLoadAnimation_Implementation();
	if (IsScreenAnimationPlaying())
	{
		return;
	}
    if (MaskOverlay.IsValid())
	{
		MaskOverlay->SetRenderOpacity(0.0f);
	}
}

void UBlackLoadingScreenWidget::StartUnloadAnimation_Implementation()
{
	Super::StartUnloadAnimation_Implementation();
	if (IsScreenAnimationPlaying())
	{
		return;
	}

    if (MaskOverlay.IsValid())
	{
		MaskOverlay->SetRenderOpacity(1.0f);
	}
}

void UBlackLoadingScreenWidget::TickAnimation(float InDeltaTime)
{
    Super::TickAnimation(InDeltaTime);
	if (!IsScreenAnimationPlaying())
	{
		return;
	}
    if (!MaskOverlay.IsValid())
	{
		return;
	}
    const float Duration = (AnimationState == ELoadingScreenAnimationState::LoadAnimation) ? LoadAnimationDuration : UnloadAnimationDuration;
    if (Duration > 0.0f)
	{
         // AnimationElapsed值，在基类计算了
        float Alpha = FMath::Clamp(AnimationElapsed / Duration, 0.0f, 1.0f);
		Alpha = ApplyEasing(Alpha, AnimationState);

		const float Opacity = (AnimationState == ELoadingScreenAnimationState::LoadAnimation) ? Alpha : (1.0f - Alpha);
		MaskOverlay->SetRenderOpacity(Opacity);
    }
}

void UBlackLoadingScreenWidget::FinishLoadAnimation_Implementation()
{
	if (MaskOverlay.IsValid())
	{
		MaskOverlay->SetRenderOpacity(1.0f);
	}
	Super::FinishLoadAnimation_Implementation();
}

void UBlackLoadingScreenWidget::FinishUnloadAnimation_Implementation()
{
	if (MaskOverlay.IsValid())
	{
		MaskOverlay->SetRenderOpacity(0.0f);
	}
	Super::FinishUnloadAnimation_Implementation();
}

float UBlackLoadingScreenWidget::ApplyEasing(float Alpha, ELoadingScreenAnimationState State)
{
	switch (State)
	{
	case ELoadingScreenAnimationState::None:
        return Alpha;
	case ELoadingScreenAnimationState::LoadAnimation:
        return Alpha;
	case ELoadingScreenAnimationState::UnloadAnimation:
        return Alpha;
	default:
		return Alpha;
	}
}
