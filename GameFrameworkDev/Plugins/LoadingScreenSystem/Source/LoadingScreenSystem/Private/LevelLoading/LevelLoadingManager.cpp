// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelLoading/LevelLoadingManager.h"
#include "LevelLoading/LevelLoadingScreenWidget.h"
#include "BlackLoading/BlackLoadingProcessTask.h"
#include "LoadingScreenSettings.h"
#include "LoadingScreenInterface.h"
#include "LogLoadingScreenSystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "HAL/ThreadHeartBeat.h"
#include "Misc/ConfigCacheIni.h"
#include "PreLoadScreenManager.h"
#include "ShaderPipelineCache.h"
#include "Kismet/GameplayStatics.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelLoadingManager)



////////////////////////////////////////////////////////////////////
// ULevelLoadingManager
////////////////////////////////////////////////////////////////////

void ULevelLoadingManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 从项目设置中读取配置
	MinimumLevelLoadingScreenDisplayTimeSecs = GetDefault<ULoadingScreenSettings>()->MinimumLevelLoadingScreenDisplayTime;

	// 注册每帧 Tick，生命周期与 Subsystem 一致
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::Tick), ProgressTickInterval);

	// 订阅 PreLoadMap（带 Context，可区分 GameInstance）
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ULevelLoadingManager::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULevelLoadingManager::HandlePostLoadMap);

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载] Manager 已初始化"));
}

void ULevelLoadingManager::Deinitialize()
{
	StopBlockingInput();

	RemoveLevelWidgetFromViewport();

	bIsProgressTickEnabled = false;

	// 移除每帧 Tick
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	// 取消地图加载委托
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载] Manager 已销毁"));

	Super::Deinitialize();
}

bool ULevelLoadingManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// 仅客户端需要加载进度追踪
	const UGameInstance* GameInstance = CastChecked<UGameInstance>(Outer);
	const bool bIsServerWorld = GameInstance->IsDedicatedServerInstance();
	return !bIsServerWorld;
}

// ================================================================
// Public API
// ================================================================

float ULevelLoadingManager::GetPreciseLoadingProgress() const
{
	return CachedProgress;
}

ELevelLoadingPhase ULevelLoadingManager::GetCurrentLoadingPhase() const
{
	return CurrentLoadingPhase;
}

float ULevelLoadingManager::GetRawAsyncLoadPercentage() const
{
	return GetAsyncLoadPercentage(NAME_None);
}

bool ULevelLoadingManager::IsLevelLoadingScreenPersistent() const
{
	if (LevelLoadingScreenUserWidgetPtr)
	{
		return LevelLoadingScreenUserWidgetPtr->IsLevelLoadingScreenPersistent();
	}
	return false;
}

bool ULevelLoadingManager::IsCurrentlyLoadingMap()
{
	if (!bCurrentlyInLoadMap)
	{
		return false;
	}

	// 查表：根据当前加载的关卡，检查是否应显示加载界面
	const FLevelLoadingScreenTableRow* Row = FindLevelLoadingScreenTableRow(PreLoadMapName);
	if (Row)
	{
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString::Printf(TEXT("Table config enables level loading screen for map [%s]"), *PreLoadMapName);
		return Row->bShouldShowLevelLoadingScreen;
	}

	// 无表配置，默认显示加载界面
	DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("No table config found for current map, defaulting to show level loading screen"));
	return true;
}

// ================================================================
// Phase Management
// ================================================================

void ULevelLoadingManager::SetLoadingPhase(ELevelLoadingPhase NewPhase)
{
	if (CurrentLoadingPhase == NewPhase)
	{
		return;
	}

	const ELevelLoadingPhase OldPhase = CurrentLoadingPhase;
	CurrentLoadingPhase = NewPhase;
	PhaseStartTime = FPlatformTime::Seconds();

	// 进入 WorldInit 时暂停世界，进入 Completed 时恢复
	if (UWorld* World = GetWorld())
	{
		if (NewPhase == ELevelLoadingPhase::WorldInit)
		{
			UGameplayStatics::SetGamePaused(World, true);
		}
		else if (NewPhase == ELevelLoadingPhase::Completed)
		{
			UGameplayStatics::SetGamePaused(World, false);
		}
	}

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载] 阶段切换: %s -> %s"),
		*UEnum::GetValueAsString(OldPhase), *UEnum::GetValueAsString(NewPhase));
}

bool ULevelLoadingManager::Tick(float DeltaTime)
{
	if (bIsProgressTickEnabled)
	{
		TickProgress(DeltaTime);
	}
	if (!IsLevelLoadingScreenPersistent())
	{
		UpdateLevelLoadingScreen();
	}
	return true;
}

void ULevelLoadingManager::TickProgress(float DeltaTime)
{
	const double Elapsed = FPlatformTime::Seconds() - PhaseStartTime;

	// 阶段自动推进
	switch (CurrentLoadingPhase)
	{
	case ELevelLoadingPhase::Preparing:
	{
		if (GetAsyncLoadPercentage(NAME_None) >= 0.0f)
		{
			SetLoadingPhase(ELevelLoadingPhase::AsyncLoading);
		}
		break;
	}
	case ELevelLoadingPhase::AsyncLoading:
	{
		// 由 CalculatePhaseProgress 通过引擎真实进度驱动
		// 阶段切换由 HandlePostLoadMap 负责
		break;
	}
	case ELevelLoadingPhase::WorldInit:
	{
		// WorldInit → Finalizing：超过最小显示时长后自动收尾
		if (Elapsed >= MinimumLevelLoadingScreenDisplayTimeSecs)
		{
			SetLoadingPhase(ELevelLoadingPhase::Finalizing);
		}
		break;
	}
	case ELevelLoadingPhase::Finalizing:
	{
		// Finalizing → Completed：短暂收尾后标记完成
		if (Elapsed >= 0.3)
		{
			SetLoadingPhase(ELevelLoadingPhase::Completed);
			bCurrentlyInLoadMap = false;
		}
		break;
	}
	case ELevelLoadingPhase::Completed:
	{
		OnLoadingCompleted();
		return;
	}
	default:
		break;
	}

	// 计算并缓存当前进度
	CachedProgress = CalculatePhaseProgress();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, ProgressTickInterval, FColor::Cyan,
			FString::Printf(TEXT("[LoadingProgress] Phase: %s | %.1f%%"),
				*UEnum::GetValueAsString(CurrentLoadingPhase), CachedProgress));
	}
}

float ULevelLoadingManager::CalculatePhaseProgress() const
{
	const double Elapsed = FPlatformTime::Seconds() - PhaseStartTime;

	switch (CurrentLoadingPhase)
	{
	case ELevelLoadingPhase::Preparing:
	{
		// 准备阶段极短，用时间基线估算 0 -> PreparingWeight
		const float Ratio = FMath::Clamp(static_cast<float>(Elapsed / 0.5), 0.0f, 1.0f);
		return Ratio * PreparingWeight;
	}

	case ELevelLoadingPhase::AsyncLoading:
	{
		// 核心阶段：使用引擎 GetAsyncLoadPercentage 真实进度
		const float Raw = GetAsyncLoadPercentage(NAME_None);
		if (Raw < 0.0f)
		{
			return PreparingWeight + AsyncLoadWeight;
		}
		return PreparingWeight + (Raw / 100.0f) * AsyncLoadWeight;
	}

	case ELevelLoadingPhase::WorldInit:
	{
		// 引擎无进度回调，用时间平滑估算
		// EaseOut 曲线 (1 - (1-t)^2)：前快后慢，保证进度始终增长
		const float Ratio = FMath::Clamp(static_cast<float>(Elapsed / MinimumLevelLoadingScreenDisplayTimeSecs), 0.0f, 1.0f);
		const float Smoothed = 1.0f - FMath::Square(1.0f - Ratio);
		return PreparingWeight + AsyncLoadWeight + Smoothed * WorldInitWeight;
	}

	case ELevelLoadingPhase::Finalizing:
	{
		// 收尾阶段：平滑接近 100%
		const float Ratio = FMath::Clamp(static_cast<float>(Elapsed / 0.3), 0.0f, 1.0f);
		return PreparingWeight + AsyncLoadWeight + WorldInitWeight + Ratio * FinalizeWeight;
	}

	default:
		return 100.0f;
	}
}

// ================================================================
// Map Loading Callbacks
// ================================================================

void ULevelLoadingManager::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		return;
	}

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载] PreLoadMap: 地图=[%s]"), *MapName);

	bCurrentlyInLoadMap = true;
	PreLoadMapName = MapName;
	OnLoadingStarted();
}

void ULevelLoadingManager::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld == nullptr || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// 计算引擎实际加载耗时（PreLoadMap → PostLoadMap）
	LevelLoadingDuration = static_cast<float>(FPlatformTime::Seconds() - LoadingStartTime);

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载] PostLoadMap: 世界=[%s] | 引擎加载耗时: %.2f 秒"), *GetNameSafe(LoadedWorld), LevelLoadingDuration);

	// 进入世界初始化阶段
	SetLoadingPhase(ELevelLoadingPhase::WorldInit);
}

void ULevelLoadingManager::OnLoadingStarted()
{
	CachedProgress = 0.0f;
	bIsProgressTickEnabled = true;
	LoadingStartTime = FPlatformTime::Seconds();

	// 进入准备阶段
	SetLoadingPhase(ELevelLoadingPhase::Preparing);

	UpdateLevelLoadingScreen();
}


void ULevelLoadingManager::OnLoadingCompleted()
{
	CachedProgress = 100.0f;
	bIsProgressTickEnabled = false;
	const float TotalDuration = static_cast<float>(FPlatformTime::Seconds() - LoadingStartTime);
	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载] 加载完成 | 引擎加载: %.2f 秒 | 总耗时: %.2f 秒 | 地图: [%s]"),
		LevelLoadingDuration, TotalDuration, *PreLoadMapName);
}

// ================================================================
// Level Loading Screen UI
// ================================================================

void ULevelLoadingManager::UpdateLevelLoadingScreen()
{
	const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();
	bool bLogLoadingScreenStatus = Settings->bLogLoadingScreenReasonEveryFrame;

	if (ShouldShowLevelLoadingScreen())
	{
		ShowLevelLoadingScreen();
	}
	else
	{
		HideLevelLoadingScreen();
	}

	if (bLogLoadingScreenStatus)
	{
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载] 显示状态: %d, 原因: %s"), bCurrentlyShowingLevelLoadingScreen ? 1 : 0, *DebugReasonForShowingOrHidingLevelLoadingScreen);
	}
}

bool ULevelLoadingManager::CheckForAnyNeedToShowLevelLoadingScreen()
{
	// 默认未知原因，后续各检查会覆盖
	DebugReasonForShowingOrHidingLevelLoadingScreen = TEXT("Reason for Showing/Hiding LevelLoadingScreen is unknown!");

	const UGameInstance* LocalGameInstance = GetGameInstance();

	if (GetDefault<ULoadingScreenSettings>()->bForceLevelLoadingScreenVisible)
	{
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("bForceLevelLoadingScreenVisible 为 true"));
		return true;
	}

	const FWorldContext* Context = LocalGameInstance->GetWorldContext();
	if (Context == nullptr)
	{
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("The game instance has a null WorldContext"));
		return true;
	}

	UWorld* World = Context->World();
	if (World == nullptr)
	{
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("We have no world (FWorldContext's World() is null)"));
		return true;
	}

	AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
	if (GameState == nullptr)
	{
		// GameState 尚未复制——仅在确实处于关卡加载流程中时显示加载界面
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("GameState hasn't yet replicated (it's null)"));
		return true;
	}


	if (IsLevelLoadingScreenPersistent())
	{
		// 关卡加载界面正在处于常驻打开
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("Level loading screen is persistently open (bCurrentlyInLoadMap is true)"));
		return true;
	}

	if (!Context->TravelURL.IsEmpty())
	{
		// 有待处理的 Travel
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("We have pending travel (the TravelURL is not empty)"));
		return true;
	}

	if (Context->PendingNetGame != nullptr)
	{
		// 正在连接到另一台服务器
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("We are connecting to another server (PendingNetGame != nullptr)"));
		return true;
	}

	if (!World->HasBegunPlay())
	{
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("World hasn't begun play"));
		return true;
	}

	if (World->IsInSeamlessTravel())
	{
		// 无缝旅行期间
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("We are in seamless travel"));
		return true;
	}

	// 询问 GameState 是否需要加载界面
	if (ILevelLoadingScreenInterface::ShouldShowLevelLoadingScreen(GameState, /*out*/ DebugReasonForShowingOrHidingLevelLoadingScreen))
	{
		return true;
	}

	// 询问 GameState 的所有组件是否需要加载界面
	for (UActorComponent* TestComponent : GameState->GetComponents())
	{
		if (ILevelLoadingScreenInterface::ShouldShowLevelLoadingScreen(TestComponent, /*out*/ DebugReasonForShowingOrHidingLevelLoadingScreen))
		{
			return true;
		}
	}

	// 检查每个 LocalPlayer
	bool bFoundAnyLocalPC = false;
	bool bMissingAnyLocalPC = false;

	for (ULocalPlayer* LP : LocalGameInstance->GetLocalPlayers())
	{
		if (LP != nullptr)
		{
			if (APlayerController* PC = LP->PlayerController)
			{
				bFoundAnyLocalPC = true;

			// 询问 PC 本身是否需要加载界面
			if (ILevelLoadingScreenInterface::ShouldShowLevelLoadingScreen(PC, /*out*/ DebugReasonForShowingOrHidingLevelLoadingScreen))
			{
				return true;
			}

			// 询问 PC 的所有组件是否需要加载界面
			for (UActorComponent* TestComponent : PC->GetComponents())
			{
				if (ILevelLoadingScreenInterface::ShouldShowLevelLoadingScreen(TestComponent, /*out*/ DebugReasonForShowingOrHidingLevelLoadingScreen))
				{
					return true;
				}
			}
			}
			else
			{
				bMissingAnyLocalPC = true;
			}
		}
	}

	UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();
	const bool bIsInSplitscreen = GameViewportClient->GetCurrentSplitscreenConfiguration() != ESplitScreenType::None;

	// 分屏模式下需要所有玩家控制器都存在
	if (bIsInSplitscreen && bMissingAnyLocalPC)
	{
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("At least one missing local player controller in splitscreen"));
		return true;
	}

	// 非分屏模式下至少需要一个玩家控制器
	if (!bIsInSplitscreen && !bFoundAnyLocalPC)
	{
		DebugReasonForShowingOrHidingLevelLoadingScreen = FString(TEXT("Need at least one local player controller"));
		return true;
	}

	// 无需显示
	DebugReasonForShowingOrHidingLevelLoadingScreen = TEXT("(nothing wants to show it anymore)");
	return false;
}

bool ULevelLoadingManager::ShouldShowLevelLoadingScreen()
{
	const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();

	// 命令行强制关闭
#if !UE_BUILD_SHIPPING
	static bool bCmdLineNoLoadingScreen = FParse::Param(FCommandLine::Get(), TEXT("NoLoadingScreen"));
	if (bCmdLineNoLoadingScreen)
	{
		return false;
	}
#endif

	// 无游戏视口时无法显示
	UGameInstance* LocalGameInstance = GetGameInstance();
	if (LocalGameInstance->GetGameViewportClient() == nullptr)
	{
		return false;
	}

	if(!IsCurrentlyLoadingMap())
	{
		return false;
	}

	// 检查是否有加载需求
	const bool bNeedToShowLoadingScreen = CheckForAnyNeedToShowLevelLoadingScreen();

	// 加载完成后再保持一段时间（覆盖流送，避免模糊）
	bool bWantToForceShowLoadingScreen = false;
	if (bNeedToShowLoadingScreen)
	{
		// 仍需显示
		TimeLoadingScreenLastDismissed = -1.0;
	}
	else
	{
		const double CurrentTime = FPlatformTime::Seconds();
		const bool bCanHoldLoadingScreen = (!GIsEditor || Settings->HoldBlackLoadingScreenAdditionalSecsEvenInEditor);
		const double HoldLoadingScreenAdditionalSecs = bCanHoldLoadingScreen ? MinimumLevelLoadingScreenDisplayTimeSecs : 0.0f;

		if (TimeLoadingScreenLastDismissed < 0.0)
		{
			TimeLoadingScreenLastDismissed = CurrentTime;
		}
		const double TimeSinceScreenDismissed = CurrentTime - TimeLoadingScreenLastDismissed;

		if ((HoldLoadingScreenAdditionalSecs > 0.0) && (TimeSinceScreenDismissed < HoldLoadingScreenAdditionalSecs))
		{
			UGameViewportClient* GameViewportClient = GetGameInstance()->GetGameViewportClient();
			GameViewportClient->bDisableWorldRendering = false;

			DebugReasonForShowingOrHidingLevelLoadingScreen = FString::Printf(TEXT("Keeping loading screen up for an additional %.2f seconds to allow texture streaming"), HoldLoadingScreenAdditionalSecs);
			bWantToForceShowLoadingScreen = true;
		}
	}

	return bNeedToShowLoadingScreen || bWantToForceShowLoadingScreen;
}

bool ULevelLoadingManager::IsShowingInitialLevelLoadingScreen() const
{
	FPreLoadScreenManager* PreLoadScreenManager = FPreLoadScreenManager::Get();
	return (PreLoadScreenManager != nullptr) && PreLoadScreenManager->HasValidActivePreLoadScreen();
}

void ULevelLoadingManager::ShowLevelLoadingScreen()
{
	if (bCurrentlyShowingLevelLoadingScreen)
	{
		return;
	}

	// 引擎仍在显示其加载界面时无法显示加载界面。
	if (IsShowingInitialLevelLoadingScreen())
	{
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] InitialLoadingScreen 显示中，等待其完成"));
	}
	else
	{
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 开始显示关卡加载界面"));

		UGameInstance* LocalGameInstance = GetGameInstance();
		const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();

		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 显示关卡加载界面"));

		// 创建加载界面控件 — 优先使用配置的 UMG 子类，留空时回退到 ULevelLoadingScreenWidget 基类
		TSubclassOf<UUserWidget> LoadingScreenWidgetClass = Settings->LevelLoadingScreenWidget.TryLoadClass<UUserWidget>();
		if (!LoadingScreenWidgetClass)
		{
			UE_LOG(LogLevelLoading, Warning, TEXT("[关卡加载界面] 未配置 Widget 类，使用默认 Widget"));
			LoadingScreenWidgetClass = ULevelLoadingScreenWidget::StaticClass();
		}

		UUserWidget* UserWidget = UUserWidget::CreateWidgetInstance(*LocalGameInstance, LoadingScreenWidgetClass, NAME_None);
		if (ULevelLoadingScreenWidget* LoadingScreenWidgetInstance = Cast<ULevelLoadingScreenWidget>(UserWidget))
		{
			UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] Widget 创建成功"));

			TimeLevelLoadingScreenShown = FPlatformTime::Seconds();

			bCurrentlyShowingLevelLoadingScreen = true;
			bIsHidingLevelLoadingScreen = false;
			LevelLoadingScreenUserWidgetPtr = LoadingScreenWidgetInstance;
			LevelLoadingScreenWidget = UserWidget->TakeWidget();

			// 绑定动画完成回调
			LoadingScreenWidgetInstance->OnLoadAnimationCompleted.AddDynamic(this, &ULevelLoadingManager::HandleLevelLoadingScreenLoadAnimationCompleted);
			LoadingScreenWidgetInstance->OnUnloadAnimationCompleted.AddDynamic(this, &ULevelLoadingManager::HandleLevelLoadingScreenUnloadAnimationCompleted);

			LoadingScreenWidgetInstance->StartLoadAnimation();

			// 添加到视口，高 ZOrder 确保在最上层
			UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();
			GameViewportClient->AddViewportWidgetContent(LevelLoadingScreenWidget.ToSharedRef(), Settings->LevelLoadingScreenZOrder);

			ChangePerformanceSettings(/*bEnableLevelLoadingScreen=*/ true);

			// 拦截输入
			StartBlockingInput();

			LevelLoadingScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ true);

			if (!GIsEditor || Settings->ForceTickLoadingScreenEvenInEditor)
			{
				// Tick Slate 以确保加载界面立即显示
				FSlateApplication::Get().Tick();
			}
		}
		else
		{
			UE_LOG(LogLevelLoading, Error, TEXT("[关卡加载界面] Widget 实例创建失败"));
		}
	}
}

void ULevelLoadingManager::HideLevelLoadingScreen()
{
	if (!bCurrentlyShowingLevelLoadingScreen)
	{
		return;
	}

	// 防止 Tick 重复进入：卸载动画播放期间 bCurrentlyShowingLevelLoadingScreen 仍为 true，
	// 而 LevelLoadingScreenUserWidgetPtr 也仍然有效，会导致每帧都重复调用 StartUnloadAnimation()
	// 和 OpenBlackLoadingScreen()，产生 BlackScreen "(reopened)" 的日志噪音和 Widget 反复创建开销
	if (bIsHidingLevelLoadingScreen)
	{
		return;
	}

	if (LevelLoadingScreenUserWidgetPtr)
	{
		bIsHidingLevelLoadingScreen = true;

		LevelLoadingScreenUserWidgetPtr->StartUnloadAnimation();
	}
	else
	{
		FinishLevelLoadingScreenCleanup();
	}
}

void ULevelLoadingManager::FinishLevelLoadingScreenCleanup()
{
	StopBlockingInput();

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 最终清理完成"));

	ChangePerformanceSettings(/*bEnableLevelLoadingScreen=*/ false);

	RemoveLevelWidgetFromViewport();

	LevelLoadingScreenUserWidgetPtr = nullptr;

	LevelLoadingScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ false);

	bCurrentlyShowingLevelLoadingScreen = false;
	bIsHidingLevelLoadingScreen = false;
}

void ULevelLoadingManager::RemoveLevelWidgetFromViewport()
{
	UGameInstance* LocalGameInstance = GetGameInstance();
	if (LevelLoadingScreenWidget.IsValid())
	{
		if (UGameViewportClient* ViewportClient = LocalGameInstance->GetGameViewportClient())
		{
			ViewportClient->RemoveViewportWidgetContent(LevelLoadingScreenWidget.ToSharedRef());
		}
		LevelLoadingScreenWidget.Reset();
	}
}

void ULevelLoadingManager::StartBlockingInput()
{
	if (!InputPreProcessor.IsValid())
	{
		InputPreProcessor = MakeShareable<FLoadingScreenInputPreProcessor>(new FLoadingScreenInputPreProcessor());
		FSlateApplication::Get().RegisterInputPreProcessor(InputPreProcessor, 0);
	}
}

void ULevelLoadingManager::StopBlockingInput()
{
	if (InputPreProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputPreProcessor);
		InputPreProcessor.Reset();
	}
}

void ULevelLoadingManager::ChangePerformanceSettings(bool bEnableLevelLoadingScreen)
{
	UGameInstance* LocalGameInstance = GetGameInstance();
	UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();

	FShaderPipelineCache::SetBatchMode(bEnableLevelLoadingScreen ? FShaderPipelineCache::BatchMode::Fast : FShaderPipelineCache::BatchMode::Background);

	// 加载界面显示时跳过 3D 世界渲染
	GameViewportClient->bDisableWorldRendering = bEnableLevelLoadingScreen;

	// 确保加载界面显示时优先流式加载关卡
	if (UWorld* ViewportWorld = GameViewportClient->GetWorld())
	{
		if (AWorldSettings* WorldSettings = ViewportWorld->GetWorldSettings(false, false))
		{
			WorldSettings->bHighPriorityLoadingLocal = bEnableLevelLoadingScreen;
		}
	}

	if (bEnableLevelLoadingScreen)
	{
		// 加载界面可见时设置挂起检测超时倍数，避免误报
		double HangDurationMultiplier;
		if (!GConfig || !GConfig->GetDouble(TEXT("Core.System"), TEXT("LoadingScreenHangDurationMultiplier"), /*out*/ HangDurationMultiplier, GEngineIni))
		{
			HangDurationMultiplier = 1.0;
		}
		FThreadHeartBeat::Get().SetDurationMultiplier(HangDurationMultiplier);

		// 加载界面显示期间不报告卡顿
		FGameThreadHitchHeartBeat::Get().SuspendHeartBeat();
	}
	else
	{
		// 隐藏加载界面时恢复挂起检测超时
		FThreadHeartBeat::Get().SetDurationMultiplier(1.0);

		// 恢复卡顿报告
		FGameThreadHitchHeartBeat::Get().ResumeHeartBeat();
	}
}

void ULevelLoadingManager::HandleLevelLoadingScreenLoadAnimationCompleted()
{
	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 淡入动画完成"));
}

void ULevelLoadingManager::HandleLevelLoadingScreenUnloadAnimationCompleted()
{
	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面] 淡出动画完成"));
	FinishLevelLoadingScreenCleanup();
}

// ================================================================
// Level Loading Screen Table
// ================================================================

const FLevelLoadingScreenTableRow* ULevelLoadingManager::FindLevelLoadingScreenTableRow(const FString& MapName)
{
	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] ===== 开始查找关卡加载界面配置 ======"));
	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] 输入 MapName: %s"), *MapName);

	const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();
	if (!Settings)
	{
		UE_LOG(LogLevelLoading, Warning, TEXT("[关卡加载界面|查表] Settings 为 nullptr，返回 nullptr"));
		return nullptr;
	}

	if (Settings->LevelLoadingScreenOverrideTable.IsNull())
	{
		UE_LOG(LogLevelLoading, Warning, TEXT("[关卡加载界面|查表] LevelLoadingScreenOverrideTable 为空 (IsNull=true)，跳过查表，返回 nullptr"));
		return nullptr;
	}

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] 数据表路径: %s"), *Settings->LevelLoadingScreenOverrideTable.ToString());

	// 懒加载缓存 DataTable，避免每次查表都 TryLoad
	if (!CachedLevelLoadingScreenOverrideTable)
	{
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] 缓存未命中，开始加载数据表..."));
		CachedLevelLoadingScreenOverrideTable = Cast<UDataTable>(Settings->LevelLoadingScreenOverrideTable.TryLoad());
		if (!CachedLevelLoadingScreenOverrideTable)
		{
			UE_LOG(LogLevelLoading, Warning, TEXT("[关卡加载界面|查表] 数据表加载失败 (TryLoad+Cast 返回 nullptr): %s"), *Settings->LevelLoadingScreenOverrideTable.ToString());
			return nullptr;
		}
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] 数据表加载成功，已缓存"));
	}
	else
	{
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] 命中缓存，跳过加载"));
	}

	UDataTable* DataTable = CachedLevelLoadingScreenOverrideTable.Get();
	if (!DataTable)
	{
		UE_LOG(LogLevelLoading, Warning, TEXT("[关卡加载界面|查表] CachedLevelLoadingScreenOverrideTable.Get() 返回 nullptr"));
		return nullptr;
	}

	// 遍历 DataTable 的所有行，匹配关卡名
	const FString MapAssetName = FPaths::GetBaseFilename(MapName);
	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] 提取的关卡资源名: %s"), *MapAssetName);

	const FLevelLoadingScreenTableRow* FoundRow = nullptr;
	int32 RowCount = 0;
	TArray<FString> MatchedAttempts;

	static const FString ContextStr(TEXT("ULevelLoadingManager::FindLevelLoadingScreenTableRow"));
	DataTable->ForeachRow<FLevelLoadingScreenTableRow>(ContextStr,
		[&MapAssetName, &FoundRow, &RowCount, &MatchedAttempts](const FName& Key, const FLevelLoadingScreenTableRow& Value)
		{
			RowCount++;
			const FString RowLevelAssetName = Value.LevelMap.GetAssetName();
			const bool bMatch = RowLevelAssetName.Equals(MapAssetName, ESearchCase::IgnoreCase);

			if (bMatch)
			{
				UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] ★ 匹配成功! Row[%d] Key='%s', LevelMap='%s' ←→ 目标='%s'"),
					RowCount, *Key.ToString(), *RowLevelAssetName, *MapAssetName);
				FoundRow = &Value;
			}
			else
			{
				UE_LOG(LogLevelLoading, Verbose, TEXT("[关卡加载界面|查表]   不匹配 Row[%d] Key='%s', LevelMap='%s' != 目标='%s'"),
					RowCount, *Key.ToString(), *RowLevelAssetName, *MapAssetName);
			}
		});

	UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] 遍历完成: 总行数=%d, 查找目标=%s"), RowCount, *MapAssetName);

	if (FoundRow)
	{
		const FString FoundAssetName = FoundRow->LevelMap.GetAssetName();
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] ✓ 找到匹配行! LevelMap=%s, OverrideConfig有效性检查通过"), *FoundAssetName);
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] ===== 查找结束 (命中) ====="));
	}
	else
	{
		UE_LOG(LogLevelLoading, Warning, TEXT("[关卡加载界面|查表] ✗ 未找到匹配行。遍历了 %d 行，未匹配到 '%s'"), RowCount, *MapAssetName);
		UE_LOG(LogLevelLoading, Log, TEXT("[关卡加载界面|查表] ===== 查找结束 (未命中) ====="));
	}

	return FoundRow;
}

FLevelLoadingScreenOverrideConfig ULevelLoadingManager::GetCurrentLevelOverrideConfig()
{
	const FLevelLoadingScreenTableRow* Row = FindLevelLoadingScreenTableRow(PreLoadMapName);
	if (Row)
	{
		return Row->OverrideConfig;
	}
	return FLevelLoadingScreenOverrideConfig();
}