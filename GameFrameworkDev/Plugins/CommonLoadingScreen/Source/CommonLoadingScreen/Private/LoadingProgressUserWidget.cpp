// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingProgressUserWidget.h"
#include "CommonLoadingScreenLog.h"
#include "CommonLoadingScreenSettings.h"
#include "LevelLoadingProgressSubsystem.h"
#include "LoadingProcessInterface.h"
#include "LoadingScreenManager.h"
#include "MoviePlayer.h"
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
	BackgroundBrush.TintColor = FLinearColor::White;
	
	VideoImageBrush.DrawAs = ESlateBrushDrawType::Image;
	VideoImageBrush.TintColor = FLinearColor::White;
}

void ULoadingProgressUserWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 根据内容类型显示/隐藏图层
	ApplyContentTypeVisibility();
	
	if (MaskOverlay.IsValid())
	{
		MaskOverlay->SetRenderOpacity(0.0f);
	}
}

void ULoadingProgressUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 解析加载界面配置（优先 Experience 覆盖，否则全局 Settings）
	ResolveLoadingScreenConfig();

	// 根据内容类型显示/隐藏图层
	ApplyContentTypeVisibility();

	// 图片类型时加载背景图
	LoadBackgroundImage();

	// 视频类型时使用 MoviePlayer 播放视频
	PlayLoadingVideo();

	// 加载动画准备
	PrepareLoadAnimation();

	// 启动独立于世界暂停的 Ticker，驱动进度更新和遮罩动画
	StartTicker();
}

void ULoadingProgressUserWidget::PrepareLoadAnimation()
{
	MaskFadeElapsed = 0.0f;
	SmoothedProgressTime = 0.0f;

	if (MaskOverlay.IsValid())
	{
		MaskOverlay->SetRenderOpacity(1.0f);
		MaskOverlay->SetRenderTransform(FSlateRenderTransform());
	}
}

void ULoadingProgressUserWidget::PrepareUnloadAnimation()
{
	MaskFadeElapsed = 0.0f;
	SmoothedProgressTime = 0.0f;

	if (MaskOverlay.IsValid())
	{
		MaskOverlay->SetRenderOpacity(0.0f);
	}
}

void ULoadingProgressUserWidget::ApplyContentTypeVisibility()
{
	if (LoadingScreenContentType == ELoadingScreenContentType::Image)
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("[LoadProgress] ContentType=Image, show ProgressOverlay, hide VideoCanvas"));
		// Image 类型：显示进度层，隐藏视频层
		if (ProgressOverlay.IsValid())
		{
			ProgressOverlay->SetVisibility(EVisibility::SelfHitTestInvisible);
		}
		if (VideoCanvas.IsValid())
		{
			VideoCanvas->SetVisibility(EVisibility::Collapsed);
		}
	}
	else if (LoadingScreenContentType == ELoadingScreenContentType::Video)
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("[LoadProgress] ContentType=Video, hide ProgressOverlay, show VideoCanvas"));
		// Video 类型：隐藏进度层，显示视频层
		if (ProgressOverlay.IsValid())
		{
			ProgressOverlay->SetVisibility(EVisibility::Collapsed);
		}
		if (VideoCanvas.IsValid())
		{
			VideoCanvas->SetVisibility(EVisibility::SelfHitTestInvisible);
		}
	}
}

void ULoadingProgressUserWidget::LoadBackgroundImage()
{
	if (LoadingScreenContentType != ELoadingScreenContentType::Image)
	{
		return;
	}

	if (LoadingScreenImageBackground.IsNull())
	{
		UE_LOG(LogLoadingScreen, Warning, TEXT("[LoadProgress] LoadingScreenImageBackground is null, skip background image"));
		return;
	}

	UTexture2D* BGTexture = Cast<UTexture2D>(LoadingScreenImageBackground.TryLoad());
	if (!BGTexture)
	{
		UE_LOG(LogLoadingScreen, Error, TEXT("[LoadProgress] Failed to load background texture: %s"), *LoadingScreenImageBackground.ToString());
		return;
	}

	BackgroundBrush.SetResourceObject(BGTexture);
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	if (BackgroundImage.IsValid())
	{
		BackgroundImage->SetImage(&BackgroundBrush);
		UE_LOG(LogLoadingScreen, Log, TEXT("[LoadProgress] Background image loaded: %s"), *LoadingScreenImageBackground.ToString());
	}
}

void ULoadingProgressUserWidget::PlayLoadingVideo()
{
	if (LoadingScreenContentType != ELoadingScreenContentType::Video)
	{
		return;
	}

	if (LoadingScreenVideoPath.IsEmpty())
	{
		UE_LOG(LogLoadingScreen, Warning, TEXT("[LoadProgress] LoadingScreenVideoPath is empty, skip video playback"));
		return;
	}

	UE_LOG(LogLoadingScreen, Log, TEXT("[LoadProgress] Playing loading video: %s, MinDisplayTime=%.1fs"),
		*LoadingScreenVideoPath, MinimumLoadingScreenDisplayTimeSecs);

	FLoadingScreenAttributes LoadingScreen;
	LoadingScreen.bAutoCompleteWhenLoadingCompletes = false;
	LoadingScreen.bMoviesAreSkippable = true;
	LoadingScreen.bWaitForManualStop = false;
	LoadingScreen.PlaybackType = EMoviePlaybackType::MT_Normal;

	LoadingScreen.MoviePaths.Add(LoadingScreenVideoPath);

	// 将自身 Widget 作为加载界面
	LoadingScreen.WidgetLoadingScreen = TakeWidget();

	// 监听视频播放完成
	GetMoviePlayer()->OnMoviePlaybackFinished().AddUObject(this, &ULoadingProgressUserWidget::OnLoadingMovieFinished);

	GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);

	UE_LOG(LogLoadingScreen, Log, TEXT("[LoadProgress] MoviePlayer setup complete, registered OnLoadingMovieFinished delegate"));
}

void ULoadingProgressUserWidget::OnLoadingMovieFinished()
{
	UE_LOG(LogLoadingScreen, Log, TEXT("[LoadProgress] Movie playback finished, preparing to hide loading screen"));

	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULoadingScreenManager* Manager = GI->GetSubsystem<ULoadingScreenManager>())
		{
			Manager->PrepareHideLoadingScreen();
		}
	}
}

void ULoadingProgressUserWidget::ResolveLoadingScreenConfig()
{
	const UCommonLoadingScreenSettings* GlobalSettings = GetDefault<UCommonLoadingScreenSettings>();

	// 通过 ILoadingProcessInterface 获取 Experience 级别的覆盖参数（零反射，纯虚函数调用）
	const FLoadingScreenOverrideConfig Override = ILoadingProcessInterface::GetLoadingScreenOverrideConfig(this);

	// --- Timing 参数 ---
	if (Override.bOverrideTiming)
	{
		LoadingScreenLoadDuration          = Override.LoadDuration;
		LoadingScreenUnloadDuration        = Override.UnloadDuration;
		LoadingScreenAnimationType         = static_cast<ELoadingAnimationType>(Override.AnimationType);
		LoadingScreenAnimationMode         = static_cast<ELoadingAnimationMode>(Override.AnimationMode);
		MinimumLoadingScreenDisplayTimeSecs = Override.MinimumLoadingScreenDisplayTime;
	}
	else
	{
		LoadingScreenLoadDuration          = GlobalSettings->LoadingScreenLoadDuration;
		LoadingScreenUnloadDuration        = GlobalSettings->LoadingScreenUnloadDuration;
		LoadingScreenAnimationType         = GlobalSettings->LoadingScreenAnimationType;
		LoadingScreenAnimationMode         = GlobalSettings->LoadingScreenAnimationMode;
		MinimumLoadingScreenDisplayTimeSecs = GlobalSettings->MinimumLoadingScreenDisplayTime;
	}

	// --- Content 参数 ---
	if (Override.bOverrideContent)
	{
		LoadingScreenContentType     = static_cast<ELoadingScreenContentType>(Override.ContentType);
		LoadingScreenImageBackground = Override.ImageBackground;
		LoadingScreenVideoPath       = Override.VideoPath;
	}
	else
	{
		LoadingScreenContentType     = GlobalSettings->LoadingScreenContentType;
		LoadingScreenImageBackground = GlobalSettings->LoadingScreenImageBackground;
		LoadingScreenVideoPath       = GlobalSettings->LoadingScreenVideoPath;
	}
}

TSharedRef<SWidget> ULoadingProgressUserWidget::RebuildWidget()
{
	RootOverlay = SNew(SOverlay)
		// --- 进度层：背景图 + 蓝图内容 ---
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(ProgressOverlay, SOverlay)
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
		]
		// --- 视频层：全局展开 ---
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(VideoCanvas, SCanvas)
			+ SCanvas::Slot()
			.Position(FVector2D(0, 0))
			.Size(FVector2D(1, 1))
			[
				SAssignNew(VideoImage, SImage)
				.Image(&VideoImageBrush)
			]
		]
		// --- 遮罩层：渐入渐出动画 ---
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
	ProgressOverlay.Reset();
	BackgroundImage.Reset();
	VideoCanvas.Reset();
	VideoImage.Reset();
	MaskOverlay.Reset();
	MaskBorder.Reset();
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

void ULoadingProgressUserWidget::SetVideoImageBrush(const FSlateBrush& InBrush)
{
	VideoImageBrush = InBrush;
	if (VideoImage.IsValid())
	{
		VideoImage->SetImage(&VideoImageBrush);
	}
}

void ULoadingProgressUserWidget::TickProgressUpdate(float InDeltaTime)
{
	SmoothedProgressTime += InDeltaTime;

	// 从子系统获取加载进度
	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULevelLoadingProgressSubsystem* Subsys = GI->GetSubsystem<ULevelLoadingProgressSubsystem>())
		{
			const float RawProgress = Subsys->GetPreciseLoadingProgress();

			// 时间平滑上限：进度不能超过按最小显示时长线性推进的上限
			const float TimeBasedMax = MinimumLoadingScreenDisplayTimeSecs > 0.0f
				? (SmoothedProgressTime / MinimumLoadingScreenDisplayTimeSecs) * 100.0f
				: 100.0f;
			const float ClampedProgress = FMath::Min(RawProgress, TimeBasedMax);

			SetProgress(ClampedProgress / 100.0f);

			if (ClampedProgress >= 100.0f)
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
	if (LoadingScreenContentType == ELoadingScreenContentType::Image)
	{
		TickProgressUpdate(InDeltaTime);
	}

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
	const float Duration = (MaskFadeState == EFadeEasing::EaseIn) ? LoadingScreenLoadDuration : LoadingScreenUnloadDuration;
	MaskFadeElapsed += InDeltaTime;

	if (Duration > 0.0f)
	{
		float Alpha = FMath::Clamp(MaskFadeElapsed / Duration, 0.0f, 1.0f);
		Alpha = ApplyEasing(Alpha, MaskFadeState);

		if (MaskOverlay.IsValid())
		{
			MaskOverlay->SetRenderOpacity((MaskFadeState == EFadeEasing::EaseIn) ? Alpha : (1.0f - Alpha));
		}
	}

	if (MaskFadeElapsed >= Duration)
	{
		const EFadeEasing CompletedState = MaskFadeState;
		MaskFadeState = EFadeEasing::None;

		if (CompletedState == EFadeEasing::EaseIn)
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
	if (MaskFadeState == EFadeEasing::EaseIn)
	{
		return;
	}

	MaskFadeState = EFadeEasing::EaseIn;
	// 卸载动画准备：不重置遮罩 opacity，从当前透明状态淡入
	PrepareUnloadAnimation();
}

void ULoadingProgressUserWidget::PlayLoadAnimation_Implementation()
{
	if (MaskFadeState == EFadeEasing::EaseOut)
	{
		return;
	}

	MaskFadeState = EFadeEasing::EaseOut;
	// 加载动画准备
	PrepareLoadAnimation();
}

bool ULoadingProgressUserWidget::IsFading() const
{
	return MaskFadeState != EFadeEasing::None;
}

float ULoadingProgressUserWidget::ApplyEasing(float Alpha, EFadeEasing Easing)
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

void ULoadingProgressUserWidget::OnUnloadAnimationFinished_Implementation()
{
	if (MaskOverlay.IsValid())
	{
		MaskOverlay->SetRenderOpacity(1.0f);
	}
	OnUnloadAnimationCompleted.Broadcast();
}

void ULoadingProgressUserWidget::OnLoadAnimationFinished_Implementation()
{
	if (MaskOverlay.IsValid())
	{
		MaskOverlay->SetRenderOpacity(0.0f);
	}
	OnLoadAnimationCompleted.Broadcast();
	GetMoviePlayer()->PlayMovie();
}
