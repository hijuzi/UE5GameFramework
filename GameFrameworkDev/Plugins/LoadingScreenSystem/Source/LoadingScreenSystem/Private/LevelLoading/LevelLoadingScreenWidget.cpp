// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelLoading/LevelLoadingScreenWidget.h"
#include "LevelLoading/LevelLoadingManager.h"
#include "LoadingScreenSettings.h"
#include "LoadingScreenInterface.h"
#include "LogLoadingScreenSystem.h"

#include "Engine/GameInstance.h"

#include "Engine/Texture2D.h"
#include "MoviePlayer.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"

ULevelLoadingScreenWidget::ULevelLoadingScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	BackgroundBrush.TintColor = FLinearColor::White;

	VideoImageBrush.DrawAs = ESlateBrushDrawType::Image;
	VideoImageBrush.TintColor = FLinearColor::White;
}

// =====================================================
// UUserWidget Overrides
// =====================================================

void ULevelLoadingScreenWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyContentTypeVisibility();
}

void ULevelLoadingScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 解析配置（优先 Interface 覆盖，否则全局 Settings）
	ResolveConfig();
	ApplyContentTypeVisibility();
	LoadBackgroundImage();

	// 视频类型时使用 MoviePlayer 播放视频
	PlayLoadingVideo();
}

void ULevelLoadingScreenWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void ULevelLoadingScreenWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	RootOverlay.Reset();
	ProgressOverlay.Reset();
	BackgroundImageWidget.Reset();
	VideoCanvas.Reset();
	VideoImageWidget.Reset();
}

// =====================================================
// RebuildWidget
// =====================================================

TSharedRef<SWidget> ULevelLoadingScreenWidget::RebuildWidget()
{
	RootOverlay = SNew(SOverlay)
		// --- 进度层：背景图 + 蓝图子类内容 ---
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
				SAssignNew(BackgroundImageWidget, SImage)
				.Image(&BackgroundBrush)
			]
			// 蓝图派生内容（进度条、提示文案等）
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				WidgetTree ? Super::RebuildWidget() : SNullWidget::NullWidget
			]
		]
		// --- 视频/图标层：全局展开 ---
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(VideoCanvas, SCanvas)
			+ SCanvas::Slot()
			.Position(FVector2D(0, 0))
			.Size(FVector2D(1, 1))
			[
				SAssignNew(VideoImageWidget, SImage)
				.Image(&VideoImageBrush)
			]
		];

	return RootOverlay.ToSharedRef();
}

// =====================================================
// Progress
// =====================================================

void ULevelLoadingScreenWidget::TickAnimation(float InDeltaTime)
{
    Super::TickAnimation(InDeltaTime);
	if (ContentType == ELoadingScreenContentType::Image)
	{
		TickProgressUpdate(InDeltaTime);
	}
}

void ULevelLoadingScreenWidget::TickProgressUpdate(float InDeltaTime)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	ULevelLoadingManager* LoadingManager = GameInstance->GetSubsystem<ULevelLoadingManager>();
	if (!LoadingManager)
	{
		return;
	}

	if (bLoadingCompleted)
	{
		return;
	}

	SmoothedProgressTime += InDeltaTime;

	const float RawProgress = LoadingManager->GetPreciseLoadingProgress();

	// 时间平滑上限：进度不能超过按最小显示时长线性推进的上限
	const float TimeBasedMax = MinimumDisplayTimeSecs > 0.0f
		? (SmoothedProgressTime / MinimumDisplayTimeSecs) * 100.0f
		: 100.0f;
	const float ClampedProgress = FMath::Min(RawProgress, TimeBasedMax);

	SetProgress(ClampedProgress / 100.0f);

	if (ClampedProgress >= 100.0f)
	{
		bLoadingCompleted = true;
		OnCloseLoadingScreen();
	}
}

void ULevelLoadingScreenWidget::SetProgress(float InProgress)
{
	CurrentProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
}

float ULevelLoadingScreenWidget::GetProgress() const
{
	return CurrentProgress;
}

// =====================================================
// Content
// =====================================================

void ULevelLoadingScreenWidget::SetBackgroundBrush(const FSlateBrush& InBrush)
{
	BackgroundBrush = InBrush;
	if (BackgroundImageWidget.IsValid())
	{
		BackgroundImageWidget->SetImage(&BackgroundBrush);
	}
}

void ULevelLoadingScreenWidget::SetVideoImageBrush(const FSlateBrush& InBrush)
{
	VideoImageBrush = InBrush;
	if (VideoImageWidget.IsValid())
	{
		VideoImageWidget->SetImage(&VideoImageBrush);
	}
}

// =====================================================
// Config Resolution
// =====================================================

void ULevelLoadingScreenWidget::ResolveConfig()
{
	const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();

	// 优先从 ILevelLoadingScreenInterface 获取覆盖参数
	const FLevelLoadingScreenOverrideConfig Override = ILevelLoadingScreenInterface::GetLevelLoadingScreenOverrideConfig(this);

	if (Override.bOverrideContent)
	{
		ContentType = static_cast<ELoadingScreenContentType>(Override.ContentType);
		ImageBackgroundPath = Override.ImageBackground;
		VideoPath = Override.VideoPath;
	}
	else
	{
		ContentType = Settings->LevelLoadingScreenContentType;
		ImageBackgroundPath = Settings->LevelLoadingScreenImageBackground;
		VideoPath = Settings->LevelLoadingScreenVideoPath;
	}

	MinimumDisplayTimeSecs = Settings->MinimumLevelLoadingScreenDisplayTime;

	UE_LOG(LogLoadingScreenSystem, Log, TEXT("[LevelLoadingScreenWidget] Config resolved: LoadDuration=%.2f, UnloadDuration=%.2f, ContentType=%s, MinDisplayTime=%.2f"),
		LoadAnimationDuration, UnloadAnimationDuration,
		ContentType == ELoadingScreenContentType::Image ? TEXT("Image") : TEXT("Video"),
		MinimumDisplayTimeSecs);
}

void ULevelLoadingScreenWidget::ApplyContentTypeVisibility()
{
	if (ContentType == ELoadingScreenContentType::Image)
	{
		if (ProgressOverlay.IsValid())
		{
			ProgressOverlay->SetVisibility(EVisibility::SelfHitTestInvisible);
		}
		if (VideoCanvas.IsValid())
		{
			VideoCanvas->SetVisibility(EVisibility::Collapsed);
		}
	}
	else if (ContentType == ELoadingScreenContentType::Video)
	{
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

void ULevelLoadingScreenWidget::LoadBackgroundImage()
{
	if (ContentType != ELoadingScreenContentType::Image)
	{
		return;
	}

	if (ImageBackgroundPath.IsNull())
	{
		UE_LOG(LogLoadingScreenSystem, Warning, TEXT("[LevelLoadingScreenWidget] ImageBackgroundPath is null, skip background image"));
		return;
	}

	UTexture2D* BGTexture = Cast<UTexture2D>(ImageBackgroundPath.TryLoad());
	if (!BGTexture)
	{
		UE_LOG(LogLoadingScreenSystem, Error, TEXT("[LevelLoadingScreenWidget] Failed to load background texture: %s"), *ImageBackgroundPath.ToString());
		return;
	}

	BackgroundBrush.SetResourceObject(BGTexture);
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	if (BackgroundImageWidget.IsValid())
	{
		BackgroundImageWidget->SetImage(&BackgroundBrush);
		UE_LOG(LogLoadingScreenSystem, Log, TEXT("[LevelLoadingScreenWidget] Background image loaded: %s"), *ImageBackgroundPath.ToString());
	}
}

// =====================================================
// Video Playback
// =====================================================

void ULevelLoadingScreenWidget::PlayLoadingVideo()
{
	if (ContentType != ELoadingScreenContentType::Video)
	{
		return;
	}

	if (VideoPath.IsEmpty())
	{
		UE_LOG(LogLoadingScreenSystem, Warning, TEXT("[LevelLoadingScreenWidget] VideoPath is empty, skip video playback"));
		return;
	}

	UE_LOG(LogLoadingScreenSystem, Log, TEXT("[LevelLoadingScreenWidget] Playing loading video: %s, MinDisplayTime=%.1fs"),
		*VideoPath, MinimumDisplayTimeSecs);

	FLoadingScreenAttributes LoadingScreen;
	LoadingScreen.bAutoCompleteWhenLoadingCompletes = false;
	LoadingScreen.bMoviesAreSkippable = true;
	LoadingScreen.bWaitForManualStop = false;
	LoadingScreen.PlaybackType = EMoviePlaybackType::MT_Normal;

	LoadingScreen.MoviePaths.Add(VideoPath);

	// 将自身 Widget 作为加载界面
	LoadingScreen.WidgetLoadingScreen = TakeWidget();

	// 监听视频播放完成
	GetMoviePlayer()->OnMoviePlaybackFinished().AddUObject(this, &ULevelLoadingScreenWidget::OnCloseLoadingScreen);

	GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);

	UE_LOG(LogLoadingScreenSystem, Log, TEXT("[LevelLoadingScreenWidget] MoviePlayer setup complete"));
}

void ULevelLoadingScreenWidget::OnLoadingMovieFinished()
{
	UE_LOG(LogLoadingScreenSystem, Log, TEXT("[LoadProgress] Movie playback finished, preparing to hide loading screen"));

	OnCloseLoadingScreen();
}

void ULevelLoadingScreenWidget::OnCloseLoadingScreen()
{
	UE_LOG(LogLoadingScreenSystem, Log, TEXT("[LevelLoadingScreenWidget] Close loading screen"));

	OnLoadAnimationCompleted.Broadcast();
}
