// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingScreenWidget.h"

ULoadingScreenWidget::ULoadingScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULoadingScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	StartTicker();
}

void ULoadingScreenWidget::NativeDestruct()
{
	StopTicker();
	Super::NativeDestruct();
}

void ULoadingScreenWidget::StartLoadAnimation_Implementation()
{
	if (IsScreenAnimationPlaying())
	{
		return;
	}

	AnimationState = ELoadingScreenAnimationState::LoadAnimation;
	AnimationElapsed = 0.0f;
}

void ULoadingScreenWidget::StartUnloadAnimation_Implementation()
{
	if (IsScreenAnimationPlaying())
	{
		return;
	}

	AnimationState = ELoadingScreenAnimationState::UnloadAnimation;
	AnimationElapsed = 0.0f;
}

bool ULoadingScreenWidget::IsScreenAnimationPlaying() const
{
	return AnimationState != ELoadingScreenAnimationState::None;
}

void ULoadingScreenWidget::FinishLoadAnimation_Implementation()
{
	OnLoadAnimationCompleted.Broadcast();
}

void ULoadingScreenWidget::FinishUnloadAnimation_Implementation()
{
	OnUnloadAnimationCompleted.Broadcast();
}

void ULoadingScreenWidget::TickAnimation(float InDeltaTime)
{
	if (!IsScreenAnimationPlaying())
	{
		return;
	}

	const float Duration = (AnimationState == ELoadingScreenAnimationState::LoadAnimation) ? LoadAnimationDuration : UnloadAnimationDuration;
	AnimationElapsed += InDeltaTime;

	if (AnimationElapsed >= Duration)
	{
		const ELoadingScreenAnimationState CompletedState = AnimationState;
		AnimationState = ELoadingScreenAnimationState::None;

		if (CompletedState == ELoadingScreenAnimationState::LoadAnimation)
		{
			FinishLoadAnimation();
		}
		else
		{
			FinishUnloadAnimation();
		}
	}
}

void ULoadingScreenWidget::StartTicker()
{
	if (!TickerHandle.IsValid())
	{
		TWeakObjectPtr<ULoadingScreenWidget> WeakThis(this);
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis](float DeltaTime) -> bool
			{
				ULoadingScreenWidget* StrongThis = WeakThis.Get();
				if (!StrongThis)
				{
					return false;
				}
				StrongThis->TickAnimation(DeltaTime);
				return true;
			}));
	}
}

void ULoadingScreenWidget::StopTicker()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}
