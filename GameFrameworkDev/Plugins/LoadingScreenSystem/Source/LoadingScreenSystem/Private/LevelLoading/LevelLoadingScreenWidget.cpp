// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelLoading/LevelLoadingScreenWidget.h"
#include "LevelLoading/LevelLoadingManager.h"
#include "BlackLoading/BlackLoadingManager.h"
#include "LoadingScreenSettings.h"
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
	VideoOverlay.Reset();
	VideoImageCanvas.Reset();
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
			SAssignNew(VideoOverlay, SOverlay)
			// 视频/图标 Canvas，全局展开，内部自由布局
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(VideoImageCanvas, SCanvas)
				+ SCanvas::Slot()
				.Position(FVector2D(0, 0))
				.Size(FVector2D(1, 1))
				[
					SAssignNew(VideoImageWidget, SImage)
					.Image(&VideoImageBrush)
				]
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

void ULevelLoadingScreenWidget::StartLoadAnimation_Implementation()
{
	Super::StartLoadAnimation_Implementation();
	bLoadAnimationCompleted = false;
	bLoadingCompleted = false;
	// 视频类型时使用 MoviePlayer 播放视频
	PlayLoadingVideo();

	// 动画期间隐藏进度层，黑屏过渡后再显示
	if (ProgressOverlay.IsValid())
	{
		ProgressOverlay->SetVisibility(EVisibility::Collapsed);
	}

	// 动画期间打开黑屏过渡界面（自动关闭模式：动画完成后黑屏自动消失）
	if (UGameInstance* LocalGameInstance = GetGameInstance())
	{
		if (UBlackLoadingManager* BlackMgr = LocalGameInstance->GetSubsystem<UBlackLoadingManager>())
		{
			BlackMgr->OpenBlackLoadingScreen(TEXT("Level loading screen load animation"), /*bAutoClose=*/ true);
		}
	}

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 开始淡入动画 - UI已隐藏"));
}

void ULevelLoadingScreenWidget::StartUnloadAnimation_Implementation()
{
	Super::StartUnloadAnimation_Implementation();

	// 卸载动画期间打开黑屏过渡界面（自动关闭模式：动画完成后黑屏自动消失）
	if (UGameInstance* LocalGameInstance = GetGameInstance())
	{
		if (UBlackLoadingManager* BlackMgr = LocalGameInstance->GetSubsystem<UBlackLoadingManager>())
		{
			BlackMgr->OpenBlackLoadingScreen(TEXT("Level loading screen unload animation"), /*bAutoClose=*/ true);
		}
	}

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 开始淡出动画"));
}

void ULevelLoadingScreenWidget::FinishLoadAnimation_Implementation()
{
	bLoadAnimationCompleted = true;
	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 淡入动画完成"));

	// 动画完成后恢复进度层显示
	if (ProgressOverlay.IsValid())
	{
		ProgressOverlay->SetVisibility(EVisibility::Visible);
	}

	Super::FinishLoadAnimation_Implementation();
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

	if (!bLoadAnimationCompleted)
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
		OnCloseLoadingScreen();
	}
}

bool ULevelLoadingScreenWidget::IsLevelLoadingScreenPersistent() const
{
	return !bLoadingCompleted;
}

void ULevelLoadingScreenWidget::SetProgress_Implementation(float InProgress)
{
	CurrentProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
}

float ULevelLoadingScreenWidget::GetProgress() const
{
	if (bLoadingCompleted)
	{
		return 1.0f;
	}
	else
	{
		return CurrentProgress;
	}
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

	// 优先从 DataTable 获取当前关卡的覆盖参数
	FLevelLoadingScreenOverrideConfig Override;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULevelLoadingManager* LM = GI->GetSubsystem<ULevelLoadingManager>())
		{
			Override = LM->GetCurrentLevelOverrideConfig();
		}
	}

	if (Override.bOverrideContent)
	{
		ContentType = Override.ContentType;
		ImageBackgroundPath = Override.ImageBackground;
		VideoPath = Override.VideoPath;
	}
	else
	{
		ContentType = Settings->LevelLoadingScreenContentType;
		ImageBackgroundPath = Settings->LevelLoadingScreenImageBackground;
		VideoPath = Settings->LevelLoadingScreenVideoPath;
	}

	// PIE 下视频无法播放，强制使用图片类型
	if (GetWorld() && GetWorld()->WorldType == EWorldType::PIE)
	{
		ContentType = ELoadingScreenContentType::Image;
	}

	MinimumDisplayTimeSecs = Settings->MinimumLevelLoadingScreenDisplayTime;

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 配置解析: LoadDuration=%.2f, UnloadDuration=%.2f, ContentType=%s, MinDisplayTime=%.2f"),
		LoadAnimationDuration, UnloadAnimationDuration,
		ContentType == ELoadingScreenContentType::Image ? TEXT("图片") : TEXT("视频"),
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
		if (VideoOverlay.IsValid())
		{
			VideoOverlay->SetVisibility(EVisibility::Collapsed);
		}
	}
	else if (ContentType == ELoadingScreenContentType::Video)
	{
		if (ProgressOverlay.IsValid())
		{
			ProgressOverlay->SetVisibility(EVisibility::Collapsed);
		}
		if (VideoOverlay.IsValid())
		{
			VideoOverlay->SetVisibility(EVisibility::SelfHitTestInvisible);
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
		UE_LOG(LogLevelLoading, Warning, TEXT("[关卡加载界面] 图片背景路径为空，跳过背景图加载"));
		return;
	}

	UTexture2D* BGTexture = Cast<UTexture2D>(ImageBackgroundPath.TryLoad());
	if (!BGTexture)
	{
		UE_LOG(LogLevelLoading, Error, TEXT("[关卡加载界面] 背景纹理加载失败: %s"), *ImageBackgroundPath.ToString());
		return;
	}

	BackgroundBrush.SetResourceObject(BGTexture);
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	if (BackgroundImageWidget.IsValid())
	{
		BackgroundImageWidget->SetImage(&BackgroundBrush);
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 背景图片已加载: %s"), *ImageBackgroundPath.ToString());
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
		UE_LOG(LogLevelLoading, Warning, TEXT("[关卡加载界面] 视频路径为空，跳过视频播放"));
		return;
	}

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 正在播放加载视频: %s, 最小显示时长=%.1fs"),
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
	GetMoviePlayer()->OnMoviePlaybackFinished().AddUObject(this, &ULevelLoadingScreenWidget::OnLoadingMovieFinished);

	GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] MoviePlayer 设置完成"));
	GetMoviePlayer()->PlayMovie();

	// PIE 下 MoviePlayer 实际不播放视频，用 MinimumDisplayTimeSecs 作为 Fallback 延时
	// 使用 CoreTicker 而非 World Timer，确保关卡加载期间也能正常触发
	if (MinimumDisplayTimeSecs > 0.0f)
	{
		MovieFinishedTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
			{
				TryCancelMovie();
				return false; // 一次性执行后自动注销
			}),
			MinimumDisplayTimeSecs);
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 视频Fallback Ticker已设置: %.2fs"), MinimumDisplayTimeSecs);
	}
	else
	{
		// 无最小显示时长配置，直接结束
		OnLoadingMovieFinished();
	}
}

void ULevelLoadingScreenWidget::OnLoadingMovieFinished()
{
	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 视频播放完成，准备隐藏"));
	OnCloseLoadingScreen();
}

void ULevelLoadingScreenWidget::TryCancelMovie()
{

	if (bLoadingCompleted)
	{
		return;
	}

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 尝试取消视频"));

	// 如果没有视频播放，则清理并关闭
	if (!GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 无视频播放，清理并关闭"));

		// 解绑 OnMoviePlaybackFinished 委托，避免重复触发
		GetMoviePlayer()->OnMoviePlaybackFinished().RemoveAll(this);

		OnCloseLoadingScreen();
	}
	else
	{
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 视频仍在播放，跳过清理"));
	}
}

void ULevelLoadingScreenWidget::OnCloseLoadingScreen()
{
	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 关闭加载界面"));
	
	// 清除 Fallback Ticker，避免重复调用
	if (MovieFinishedTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(MovieFinishedTickerHandle);
		MovieFinishedTickerHandle.Reset();
	}
	bLoadingCompleted = true;
}
