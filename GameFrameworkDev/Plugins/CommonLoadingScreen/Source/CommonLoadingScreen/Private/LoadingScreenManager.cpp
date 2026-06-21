// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingScreenManager.h"

#include "HAL/ThreadHeartBeat.h"

#include "BlackScreenUserWidget.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

#include "LoadingProcessInterface.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

#include "PreLoadScreen.h"
#include "PreLoadScreenManager.h"

#include "ShaderPipelineCache.h"
#include "CommonLoadingScreenSettings.h"
#include "BlackScreenUserWidget.h"

#include "LoadingProgressUserWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "UMG.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LoadingScreenManager)

DECLARE_LOG_CATEGORY_EXTERN(LogLoadingScreen, Log, All);
DEFINE_LOG_CATEGORY(LogLoadingScreen);

//@TODO: 为什么 GetLocalPlayers() 会有 nullptr 条目？真的会发生吗？
//@TODO: 在 PIE 模拟模式下测试，决定加载界面行为应该有多少（如果有的话）
//@TODO: 允许除 GameState/PlayerController（及其拥有的组件）之外的其他实现 ILoadingProcessInterface 的对象注册为感兴趣方
//@TODO: 更改音乐设置（在这里或通过 LoadingScreenVisibilityChanged 委托实现）
//@TODO: 工作室分析（FireEvent_PIEFinishedLoading / 跟踪 PIE 启动时间以监控回归，在这里或通过 LoadingScreenVisibilityChanged 委托实现）

// 加载界面性能分析类别
CSV_DEFINE_CATEGORY(LoadingScreen, true);

//////////////////////////////////////////////////////////////////////

bool ILoadingProcessInterface::ShouldShowLoadingScreen(UObject* TestObject, FString& OutReason)
{
	if (TestObject != nullptr)
	{
		if (ILoadingProcessInterface* LoadObserver = Cast<ILoadingProcessInterface>(TestObject))
		{
			FString ObserverReason;
			if (LoadObserver->ShouldShowLoadingScreen(/*out*/ ObserverReason))
			{
				if (ensureMsgf(!ObserverReason.IsEmpty(), TEXT("%s failed to set a reason why it wants to show the loading screen"), *GetPathNameSafe(TestObject)))
				{
					OutReason = ObserverReason;
				}
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////

namespace LoadingScreenCVars
{
	// 控制台变量
	static float HoldLoadingScreenAdditionalSecs = 2.0f;
	static FAutoConsoleVariableRef CVarHoldLoadingScreenUpAtLeastThisLongInSecs(
		TEXT("CommonLoadingScreen.HoldLoadingScreenAdditionalSecs"),
		HoldLoadingScreenAdditionalSecs,
		TEXT("其他加载完成后额外保持加载界面的时长（秒），以便给纹理流式加载留出时间，避免画面模糊"),
		ECVF_Default | ECVF_Preview);

	static bool LogLoadingScreenReasonEveryFrame = false;
	static FAutoConsoleVariableRef CVarLogLoadingScreenReasonEveryFrame(
		TEXT("CommonLoadingScreen.LogLoadingScreenReasonEveryFrame"),
		LogLoadingScreenReasonEveryFrame,
		TEXT("为 true 时，每帧都会将加载界面显示或隐藏的原因输出到日志。"),
		ECVF_Default);

	static bool ForceLoadingScreenVisible = false;
	static FAutoConsoleVariableRef CVarForceLoadingScreenVisible(
		TEXT("CommonLoadingScreen.AlwaysShow"),
		ForceLoadingScreenVisible,
		TEXT("强制显示加载界面。"),
		ECVF_Default);
}

//////////////////////////////////////////////////////////////////////
// FLoadingScreenInputPreProcessor

// 加载界面显示时插入的输入处理器
// 它会拦截所有输入，确保加载界面下方的活动菜单不会响应交互
class FLoadingScreenInputPreProcessor : public IInputProcessor
{
public:
	FLoadingScreenInputPreProcessor() { }
	virtual ~FLoadingScreenInputPreProcessor() { }

	bool CanEatInput() const
	{
		return !GIsEditor;
	}

	//~IInputProcess 接口
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override { }

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return CanEatInput(); }
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return CanEatInput(); }
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override { return CanEatInput(); }
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override { return CanEatInput(); }
	virtual bool HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& MotionEvent) override { return CanEatInput(); }
	//~End of IInputProcess 接口
};

//////////////////////////////////////////////////////////////////////
// ULoadingScreenManager

void ULoadingScreenManager::Initialize(FSubsystemCollectionBase& Collection)
{
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);

	const UGameInstance* LocalGameInstance = GetGameInstance();
	check(LocalGameInstance);
}

void ULoadingScreenManager::Deinitialize()
{
	StopBlockingInputForLoadingScreen();
	StopBlockingInputForBlackScreen();

	RemoveLoadingScreenWidgetFromViewport();
	RemoveBlackScreenWidgetFromViewport();

	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	// 已完成工作，不再尝试 Tick 本对象
	SetTickableTickType(ETickableTickType::Never);
}

bool ULoadingScreenManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// 仅客户端需要加载界面
	const UGameInstance* GameInstance = CastChecked<UGameInstance>(Outer);
	const bool bIsServerWorld = GameInstance->IsDedicatedServerInstance();	
	return !bIsServerWorld;
}

bool ULoadingScreenManager::IsAnyScreenFading() const
{
	return BlackScreenUserWidgetPtr && BlackScreenUserWidgetPtr->IsFading();
}

void ULoadingScreenManager::LoadBlackScreen()
{
	// 任意界面正在缓动中，不加载过度界面
	if (IsAnyScreenFading())
	{
		return;
	}

	if (ShouldShowBlackScreen())
	{
		ShowBlackScreen();
	}
	else
	{
		// 延迟到Tick中检查，设置等待
		LoadBlackScreenState = ELoadScreenState::PendingLoad;
	}
}

void ULoadingScreenManager::Tick(float DeltaTime)
{
	if (LoadBlackScreenState == ELoadScreenState::PendingLoad)
	{
		LoadBlackScreen();
	}
	else if (LoadBlackScreenState == ELoadScreenState::PendingHide)
	{
		HideBlackScreen();
	}

	if (LoadingScreenState == ELoadScreenState::PendingLoad)
	{
		LoadLoadingScreen();
	}
	else if (LoadingScreenState == ELoadScreenState::PendingHide)
	{
		HideLoadingScreen();
	}

	TimeUntilNextLogHeartbeatSeconds = FMath::Max(TimeUntilNextLogHeartbeatSeconds - DeltaTime, 0.0);

	if (LoadingScreenCVars::LogLoadingScreenReasonEveryFrame)
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("加载界面状态: %s. 原因: %s"), *UEnum::GetValueAsString(LoadingScreenState), *DebugReasonForShowingOrHidingLoadingScreen);
		UE_LOG(LogLoadingScreen, Log, TEXT("黑屏状态: %s. 原因: %s"), *UEnum::GetValueAsString(LoadBlackScreenState), *DebugReasonForBlackScreen);
	}
}

ETickableTickType ULoadingScreenManager::GetTickableTickType() const
{
	if (IsTemplate())
	{
		return ETickableTickType::Never;
	}
	return ETickableTickType::Conditional;
}

bool ULoadingScreenManager::IsTickable() const
{
	// 如果没有游戏视口客户端则不 Tick，这里捕获的是 ShouldCreateSubsystem 未覆盖的情况
	UGameInstance* GameInstance = GetGameInstance();
	return (GameInstance && GameInstance->GetGameViewportClient());
}

TStatId ULoadingScreenManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULoadingScreenManager, STATGROUP_Tickables);
}

UWorld* ULoadingScreenManager::GetTickableGameObjectWorld() const
{
	return GetGameInstance()->GetWorld();
}

void ULoadingScreenManager::RegisterLoadingProcessor(TScriptInterface<ILoadingProcessInterface> Interface)
{
	ExternalLoadingProcessors.Add(Interface.GetObject());
}

void ULoadingScreenManager::UnregisterLoadingProcessor(TScriptInterface<ILoadingProcessInterface> Interface)
{
	ExternalLoadingProcessors.Remove(Interface.GetObject());
}

void ULoadingScreenManager::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance == GetGameInstance())
	{
		bCurrentlyInLoadMap = true;
		PreLoadMapName = MapName;

		// 如果引擎已初始化，立即进入黑屏与加载界面
		if (GEngine->IsInitialized())
		{
			LoadBlackScreenState = ELoadScreenState::PendingLoad;
			LoadingScreenState = ELoadScreenState::None;
			// 需要在Tick前，就检测黑屏加载，避免Tick启动后，黑屏加载无法触发
			LoadBlackScreen();
		}
	}
}

void ULoadingScreenManager::HandlePostLoadMap(UWorld* World)
{
	if ((World != nullptr) && (World->GetGameInstance() == GetGameInstance()))
	{
		bCurrentlyInLoadMap = false;
		LoadBlackScreenState = ELoadScreenState::None;
		LoadingScreenState = ELoadScreenState::None;
		PreLoadMapName.Empty();
	}
}

void ULoadingScreenManager::LoadLoadingScreen()
{
	// 任意界面正在缓动中，不加载加载界面
	if (IsAnyScreenFading())
	{
		return;
	}

	if (ShouldShowLoadingScreenWidget())
	{
		ShowLoadingScreen();
	}
	else
	{
		// 延迟到Tick中检查，设置等待
		LoadingScreenState = ELoadScreenState::PendingLoad;
	}
}

bool ULoadingScreenManager::CheckForSystemNeedBlackScreen()
{
	// 初始化为"未知"原因，以防未来修改此处时忘记设置原因。
	DebugReasonForBlackScreen = TEXT("显示/隐藏黑屏的原因未知！");

	const UGameInstance* LocalGameInstance = GetGameInstance();

	if (LoadingScreenCVars::ForceLoadingScreenVisible)
	{
		DebugReasonForBlackScreen = FString(TEXT("CommonLoadingScreen.AlwaysShow 为 true"));
		return true;
	}

	const FWorldContext* Context = LocalGameInstance->GetWorldContext();
	if (Context == nullptr)
	{
		// 当前没有 WorldContext……最好显示黑屏
		DebugReasonForBlackScreen = FString(TEXT("游戏实例的 WorldContext 为空"));
		return true;
	}

	UWorld* World = Context->World();
	if (World == nullptr)
	{
		DebugReasonForBlackScreen = FString(TEXT("没有 World（FWorldContext 的 World() 为空）"));
		return true;
	}

	AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
	if (GameState == nullptr)
	{
		// GameState 尚未同步。
		DebugReasonForBlackScreen = FString(TEXT("GameState 尚未同步（为空）"));
		return true;
	}

	if (bCurrentlyInLoadMap)
	{
		// 处于 LoadMap 时显示黑屏
		DebugReasonForBlackScreen = FString(TEXT("bCurrentlyInLoadMap 为 true"));
		return true;
	}

	if (!Context->TravelURL.IsEmpty())
	{
		// 有待处理的场景切换时显示黑屏
		DebugReasonForBlackScreen = FString(TEXT("有待处理的场景切换（TravelURL 不为空）"));
		return true;
	}

	if (Context->PendingNetGame != nullptr)
	{
		// 正在连接到其他服务器
		DebugReasonForBlackScreen = FString(TEXT("正在连接到其他服务器（PendingNetGame != nullptr）"));
		return true;
	}

	if (!World->HasBegunPlay())
	{
		DebugReasonForBlackScreen = FString(TEXT("World 尚未开始游戏"));
		return true;
	}

	if (World->IsInSeamlessTravel())
	{
		// 无缝切换场景时显示黑屏
		DebugReasonForBlackScreen = FString(TEXT("正在进行无缝场景切换"));
		return true;
	}

	// 检查每个本地玩家的 PC 是否存在（此处不检查 ILoadingProcessInterface）
	bool bFoundAnyLocalPC = false;
	bool bMissingAnyLocalPC = false;

	for (ULocalPlayer* LP : LocalGameInstance->GetLocalPlayers())
	{
		if (LP != nullptr)
		{
			if (LP->PlayerController != nullptr)
			{
				bFoundAnyLocalPC = true;
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
		DebugReasonForBlackScreen = FString(TEXT("分屏模式下至少缺少一个本地玩家控制器"));
		return true;
	}

	// 非分屏模式下至少需要一个玩家控制器
	if (!bIsInSplitscreen && !bFoundAnyLocalPC)
	{
		DebugReasonForBlackScreen = FString(TEXT("至少需要一个本地玩家控制器"));
		return true;
	}

	// 胜利！现在可以关闭黑屏了
	DebugReasonForBlackScreen = TEXT("（系统条件已满足，无需黑屏）");
	return false;
}

bool ULoadingScreenManager::CheckForAnyLoadingProcessInterfaceNeed()
{
	// 初始化为"未知"原因，以防未来修改此处时忘记设置原因。
	DebugReasonForShowingOrHidingLoadingScreen = TEXT("显示/隐藏加载界面的原因未知！");

	const UGameInstance* LocalGameInstance = GetGameInstance();

	const FWorldContext* Context = LocalGameInstance->GetWorldContext();
	if (Context == nullptr)
	{
		return false;
	}

	UWorld* World = Context->World();
	if (World == nullptr)
	{
		return false;
	}

	AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
	if (GameState == nullptr)
	{
		return false;
	}

	// 询问 GameState 是否需要加载界面	
	if (ILoadingProcessInterface::ShouldShowLoadingScreen(GameState, /*out*/ DebugReasonForShowingOrHidingLoadingScreen))
	{
		return true;
	}

	// 询问 GameState 的所有组件是否需要加载界面
	for (UActorComponent* TestComponent : GameState->GetComponents())
	{
		if (ILoadingProcessInterface::ShouldShowLoadingScreen(TestComponent, /*out*/ DebugReasonForShowingOrHidingLoadingScreen))
		{
			return true;
		}
	}

	// 询问所有可能已注册的外部加载处理器。这些可能是游戏代码注册的 Actor 或组件，
	// 用于告知我们保持加载界面，等待某些资源流式加载完成。
	for (const TWeakInterfacePtr<ILoadingProcessInterface>& Processor : ExternalLoadingProcessors)
	{
		if (ILoadingProcessInterface::ShouldShowLoadingScreen(Processor.GetObject(), /*out*/ DebugReasonForShowingOrHidingLoadingScreen))
		{
			return true;
		}
	}

	// 检查每个本地玩家的 PC 及其组件是否实现了 ILoadingProcessInterface
	for (ULocalPlayer* LP : LocalGameInstance->GetLocalPlayers())
	{
		if (LP != nullptr)
		{
			if (APlayerController* PC = LP->PlayerController)
			{
				// 询问 PC 自身是否需要加载界面
				if (ILoadingProcessInterface::ShouldShowLoadingScreen(PC, /*out*/ DebugReasonForShowingOrHidingLoadingScreen))
				{
					return true;
				}

				// 询问 PC 的所有组件是否需要加载界面
				for (UActorComponent* TestComponent : PC->GetComponents())
				{
					if (ILoadingProcessInterface::ShouldShowLoadingScreen(TestComponent, /*out*/ DebugReasonForShowingOrHidingLoadingScreen))
					{
						return true;
					}
				}
			}
		}
	}

	// 没有 ILoadingProcessInterface 需要显示加载界面
	DebugReasonForShowingOrHidingLoadingScreen = TEXT("（没有 ILoadingProcessInterface 需要显示加载界面）");
	return false;
}


bool ULoadingScreenManager::ShouldShowBlackScreen()
{
	// 没有游戏视口时无法显示黑屏
	UGameInstance* LocalGameInstance = GetGameInstance();
	if (LocalGameInstance->GetGameViewportClient() == nullptr)
	{
		return false;
	}

	// 检查是否需要显示黑屏
	const bool bNeedToShowBlackScreen = CheckForSystemNeedBlackScreen();

	// 如果需要的话，将黑屏额外保持一段时间（纹理流式加载）
	const UCommonLoadingScreenSettings* Settings = GetDefault<UCommonLoadingScreenSettings>();
	bool bWantToForceShowBlackScreen = false;
	if (bNeedToShowBlackScreen)
	{
		// 系统仍需要黑屏，重置解除时间戳为无效值（表示黑屏处于活跃状态）
		TimeBlackScreenLastDismissed = -1.0;
	}
	else
	{
		const double CurrentTime = FPlatformTime::Seconds();
		const bool bCanHoldBlackScreen = (!GIsEditor || Settings->HoldBlackScreenAdditionalSecsEvenInEditor);
		const double HoldBlackSecs = bCanHoldBlackScreen ? Settings->HoldBlackScreenAdditionalSecs : 0.0;

		// 系统不再需要黑屏时，记录下解除时刻作为 hold 计时的起点
		// 若已过时间未达到 HoldBlackScreenAdditionalSecs，则继续显示黑屏，给纹理流式加载留出缓冲时间
		if (TimeBlackScreenLastDismissed < 0.0)
		{
			TimeBlackScreenLastDismissed = CurrentTime;
		}
		const double TimeSinceScreenDismissed = CurrentTime - TimeBlackScreenLastDismissed;

		if ((HoldBlackSecs > 0.0) && (TimeSinceScreenDismissed < HoldBlackSecs))
		{
			UGameViewportClient* GameViewportClient = GetGameInstance()->GetGameViewportClient();
			GameViewportClient->bDisableWorldRendering = false;

			DebugReasonForBlackScreen = FString::Printf(TEXT("额外保持黑屏 %.2f 秒以允许纹理流式加载"), HoldBlackSecs);
			bWantToForceShowBlackScreen = true;
		}
	}

	return bNeedToShowBlackScreen || bWantToForceShowBlackScreen;
}

bool ULoadingScreenManager::ShouldShowLoadingScreenWidget()
{
#if !UE_BUILD_SHIPPING
	static bool bCmdLineNoLoadingScreen = FParse::Param(FCommandLine::Get(), TEXT("NoLoadingScreen"));
	if (bCmdLineNoLoadingScreen)
	{
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("命令行参数包含 'NoLoadingScreen'"));
		return false;
	}
#endif

	// 没有游戏视口时无法显示加载界面控件
	UGameInstance* LocalGameInstance = GetGameInstance();
	if (LocalGameInstance->GetGameViewportClient() == nullptr)
	{
		return false;
	}

	return CheckForAnyLoadingProcessInterfaceNeed();
}

bool ULoadingScreenManager::IsShowingInitialLoadingScreen() const
{
	FPreLoadScreenManager* PreLoadScreenManager = FPreLoadScreenManager::Get();
	return (PreLoadScreenManager != nullptr) && PreLoadScreenManager->HasValidActivePreLoadScreen();
}

void ULoadingScreenManager::ShowLoadingScreen()
{
	if (LoadingScreenState == ELoadScreenState::Loaded)
	{
		return;
	}

	// 引擎仍在显示其加载界面时无法显示加载界面。
	if (FPreLoadScreenManager::Get() && FPreLoadScreenManager::Get()->HasActivePreLoadScreenType(EPreLoadScreenTypes::EngineLoadingScreen))
	{
		return;
	}

	TimeLoadingScreenShown = FPlatformTime::Seconds();

	LoadingScreenState = ELoadScreenState::Loaded;

	CSV_EVENT(LoadingScreen, TEXT("Show"));

	const UCommonLoadingScreenSettings* Settings = GetDefault<UCommonLoadingScreenSettings>();

	// 如果在指定时间内未能到达指定检查点，将触发挂起检测器
	FThreadHeartBeat::Get().MonitorCheckpointStart(GetFName(), Settings->LoadingScreenHeartbeatHangDuration);

	// 按配置的时间间隔输出心跳日志
	if ((Settings->LogLoadingScreenHeartbeatInterval > 0.0f) && (TimeUntilNextLogHeartbeatSeconds <= 0.0))
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("加载界面状态: %s. 原因: %s"), *UEnum::GetValueAsString(LoadingScreenState), *DebugReasonForShowingOrHidingLoadingScreen);
		TimeUntilNextLogHeartbeatSeconds = Settings->LogLoadingScreenHeartbeatInterval;
	}

	if (IsShowingInitialLoadingScreen())
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("在 'IsShowingInitialLoadingScreen()' 为 true 时显示加载界面。"));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForShowingOrHidingLoadingScreen);
	}
	else
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("在 'IsShowingInitialLoadingScreen()' 为 false 时显示加载界面。"));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForShowingOrHidingLoadingScreen);

		UGameInstance* LocalGameInstance = GetGameInstance();

		// 加载界面显示时拦截输入
		StartBlockingInputForLoadingScreen();

		LoadingScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ true);

		// 创建加载界面控件 — 优先使用配置的 UMG 子类，留空时回退到 ULoadingProgressUserWidget 基类
		TSubclassOf<UUserWidget> LoadingScreenWidgetClass = Settings->LoadingScreenWidget.TryLoadClass<UUserWidget>();
		if (!LoadingScreenWidgetClass)
		{
			UE_LOG(LogLoadingScreen, Warning, TEXT("未配置 LoadingScreenWidget，回退到 ULoadingProgressUserWidget。"));
			LoadingScreenWidgetClass = ULoadingProgressUserWidget::StaticClass();
		}

		UUserWidget* UserWidget = UUserWidget::CreateWidgetInstance(*LocalGameInstance, LoadingScreenWidgetClass, NAME_None);
		if (UserWidget)
		{
			UE_LOG(LogLoadingScreen, Log, TEXT("LoadingScreenUserWidget 创建成功。"));

			LoadingScreenState = ELoadScreenState::Loaded;
			LoadingScreenWidget = UserWidget->TakeWidget();

			// 以高 ZOrder 添加到视口，确保位于大多数元素之上
			UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();
			GameViewportClient->AddViewportWidgetContent(LoadingScreenWidget.ToSharedRef(), Settings->LoadingScreenZOrder);

			ChangePerformanceSettings(/*bEnableLoadingScreen=*/ true);

			if (!GIsEditor || Settings->ForceTickLoadingScreenEvenInEditor)
			{
				// Tick Slate 以确保加载界面立即显示
				FSlateApplication::Get().Tick();
			}
		}
		else
		{
			LoadingScreenState = ELoadScreenState::None;
			UE_LOG(LogLoadingScreen, Error, TEXT("无法创建 LoadingScreenUserWidget 实例。"));
		}
	}
}

void ULoadingScreenManager::HideLoadingScreen()
{
	if (LoadingScreenState == ELoadScreenState::None)
	{
		return;
	}

	StopBlockingInputForLoadingScreen();

	if (IsShowingInitialLoadingScreen())
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("在 'IsShowingInitialLoadingScreen()' 为 true 时隐藏加载界面。"));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForShowingOrHidingLoadingScreen);
	}
	else
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("在 'IsShowingInitialLoadingScreen()' 为 false 时隐藏加载界面。"));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForShowingOrHidingLoadingScreen);

		UE_LOG(LogLoadingScreen, Log, TEXT("在移除加载界面前执行垃圾回收"));
		GEngine->ForceGarbageCollection(true);

		RemoveLoadingScreenWidgetFromViewport();
	
		ChangePerformanceSettings(/*bEnableLoadingScreen=*/ false);

		// 通知观察者加载界面已结束
		LoadingScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ false);
	}

	CSV_EVENT(LoadingScreen, TEXT("Hide"));

	const double LoadingScreenDuration = FPlatformTime::Seconds() - TimeLoadingScreenShown;
	UE_LOG(LogLoadingScreen, Log, TEXT("加载界面显示了 %.2f 秒"), LoadingScreenDuration);

	LoadingScreenState = ELoadScreenState::None;

	FThreadHeartBeat::Get().MonitorCheckpointEnd(GetFName());
}

void ULoadingScreenManager::RemoveLoadingScreenWidgetFromViewport()
{
	UGameInstance* LocalGameInstance = GetGameInstance();
	if (LoadingScreenWidget.IsValid())
	{
		if (UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient())
		{
			GameViewportClient->RemoveViewportWidgetContent(LoadingScreenWidget.ToSharedRef());
		}
		LoadingScreenWidget.Reset();
	}
}

void ULoadingScreenManager::ShowBlackScreen()
{
	if (LoadBlackScreenState == ELoadScreenState::Loaded)
	{
		return;
	}

	// 引擎仍在显示其加载界面时无法显示黑屏。
	if (FPreLoadScreenManager::Get() && FPreLoadScreenManager::Get()->HasActivePreLoadScreenType(EPreLoadScreenTypes::EngineLoadingScreen))
	{
		return;
	}

	TimeBlackScreenShown = FPlatformTime::Seconds();


	CSV_EVENT(LoadingScreen, TEXT("BlackScreen Show"));

	const UCommonLoadingScreenSettings* Settings = GetDefault<UCommonLoadingScreenSettings>();

	if (IsShowingInitialLoadingScreen())
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("在 'IsShowingInitialLoadingScreen()' 为 true 时显示黑屏。"));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForBlackScreen);
	}
	else
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("在 'IsShowingInitialLoadingScreen()' 为 false 时显示黑屏。"));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForBlackScreen);

		UGameInstance* LocalGameInstance = GetGameInstance();

		// 创建黑屏控件 — 优先使用配置的 UMG 子类，留空时回退到 UBlackScreenUserWidget 基类
		TSubclassOf<UUserWidget> BlackScreenWidgetClass = Settings->BlackScreenWidget.TryLoadClass<UUserWidget>();
		if (!BlackScreenWidgetClass)
		{
			BlackScreenWidgetClass = UBlackScreenUserWidget::StaticClass();
		}
		
		UUserWidget* UserWidget = UUserWidget::CreateWidgetInstance(*LocalGameInstance, BlackScreenWidgetClass, NAME_None);
		if (UBlackScreenUserWidget* BlackScreenWidgetInstance = Cast<UBlackScreenUserWidget>(UserWidget))
		{
			UE_LOG(LogLoadingScreen, Log, TEXT("BlackScreenUserWidget 创建成功。"));

			LoadBlackScreenState = ELoadScreenState::Loaded;
			BlackScreenWidget = BlackScreenWidgetInstance->TakeWidget();
			BlackScreenUserWidgetPtr = BlackScreenWidgetInstance;

			// 绑定动画完成回调并播放淡入
			BlackScreenWidgetInstance->OnFadeInCompleted.AddDynamic(this, &ULoadingScreenManager::HandleBlackScreenFadeInCompleted);
			BlackScreenWidgetInstance->OnFadeOutCompleted.AddDynamic(this, &ULoadingScreenManager::HandleBlackScreenFadeOutCompleted);

			BlackScreenWidgetInstance->PlayFadeIn();

			// 淡入开始后拦截输入并广播可见性
			StartBlockingInputForBlackScreen();

			// 添加到视口，位于加载界面下方
			UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();
			GameViewportClient->AddViewportWidgetContent(BlackScreenWidget.ToSharedRef(), Settings->BlackScreenZOrder);

			ChangePerformanceSettingsForBlackScreen(/*bEnablingBlackScreen=*/ true);

			if (!GIsEditor || Settings->ForceTickLoadingScreenEvenInEditor)
			{
				// Tick Slate 以确保黑屏立即显示
				FSlateApplication::Get().Tick();
			}
		}
		else
		{
			LoadBlackScreenState = ELoadScreenState::None;
			UE_LOG(LogLoadingScreen, Error, TEXT("无法创建 BlackScreenUserWidget 实例。"));
		}
	}
}

void ULoadingScreenManager::HideBlackScreen()
{
	if (LoadBlackScreenState == ELoadScreenState::None)
	{
		return;
	}

	StopBlockingInputForBlackScreen();

	CSV_EVENT(LoadingScreen, TEXT("BlackScreen Hide"));

	const double BlackScreenDuration = FPlatformTime::Seconds() - TimeBlackScreenShown;
	UE_LOG(LogLoadingScreen, Log, TEXT("黑屏显示了 %.2f 秒"), BlackScreenDuration);



	if (IsShowingInitialLoadingScreen())
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("在 'IsShowingInitialLoadingScreen()' 为 true 时隐藏黑屏。"));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForBlackScreen);
		return;
	}

	UE_LOG(LogLoadingScreen, Log, TEXT("在 'IsShowingInitialLoadingScreen()' 为 false 时隐藏黑屏。"));
	UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForBlackScreen);

	// 如果有 UBlackScreenUserWidget，播放淡出动画后再清理
	if (BlackScreenUserWidgetPtr)
	{
		LoadBlackScreenState = ELoadScreenState::None;
		BlackScreenUserWidgetPtr->PlayFadeOut();
		return;
	}

	// 无 UBlackScreenUserWidget，直接清理
	FinishBlackScreenCleanup();
}

void ULoadingScreenManager::RemoveBlackScreenWidgetFromViewport()
{
	UGameInstance* LocalGameInstance = GetGameInstance();
	if (BlackScreenWidget.IsValid())
	{
		if (UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient())
		{
			GameViewportClient->RemoveViewportWidgetContent(BlackScreenWidget.ToSharedRef());
		}
		BlackScreenWidget.Reset();
		BlackScreenUserWidgetPtr = nullptr;
	}
}

void ULoadingScreenManager::FinishBlackScreenCleanup()
{
	RemoveBlackScreenWidgetFromViewport();
	ChangePerformanceSettingsForBlackScreen(/*bEnablingBlackScreen=*/ false);
	LoadBlackScreenState = ELoadScreenState::None;
	BlackScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ false);
}

void ULoadingScreenManager::HandleBlackScreenFadeInCompleted()
{
	UE_LOG(LogLoadingScreen, Log, TEXT("黑屏淡入动画完成。"));
	LoadBlackScreenState = ELoadScreenState::Loaded;
	BlackScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ true);

	if (BlackScreenUserWidgetPtr && BlackScreenUserWidgetPtr->ShouldShowLoadingScreen())
	{
		// 开始加载界面
		LoadingScreenState = ELoadScreenState::PendingLoad;
	}
	else
	{
		// 不需要加载界面，直接触发隐藏黑屏
		LoadBlackScreenState = ELoadScreenState::PendingHide;
	}
}

void ULoadingScreenManager::HandleBlackScreenFadeOutCompleted()
{
	UE_LOG(LogLoadingScreen, Log, TEXT("黑屏淡出动画完成。"));
	FinishBlackScreenCleanup();
}

void ULoadingScreenManager::StartBlockingInputForLoadingScreen()
{
	if (!InputPreProcessor.IsValid())
	{
		InputPreProcessor = MakeShareable<FLoadingScreenInputPreProcessor>(new FLoadingScreenInputPreProcessor());
		FSlateApplication::Get().RegisterInputPreProcessor(InputPreProcessor, 0);
	}
}

void ULoadingScreenManager::StopBlockingInputForLoadingScreen()
{
	if (InputPreProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputPreProcessor);
		InputPreProcessor.Reset();
	}
}

void ULoadingScreenManager::StartBlockingInputForBlackScreen()
{
	if (!BlackScreenInputPreProcessor.IsValid())
	{
		BlackScreenInputPreProcessor = MakeShareable<FLoadingScreenInputPreProcessor>(new FLoadingScreenInputPreProcessor());
		FSlateApplication::Get().RegisterInputPreProcessor(BlackScreenInputPreProcessor, 0);
	}
}

void ULoadingScreenManager::StopBlockingInputForBlackScreen()
{
	if (BlackScreenInputPreProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(BlackScreenInputPreProcessor);
		BlackScreenInputPreProcessor.Reset();
	}
}

void ULoadingScreenManager::ChangePerformanceSettings(bool bEnabingLoadingScreen)
{
	UGameInstance* LocalGameInstance = GetGameInstance();
	UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();

	FShaderPipelineCache::SetBatchMode(bEnabingLoadingScreen ? FShaderPipelineCache::BatchMode::Fast : FShaderPipelineCache::BatchMode::Background);

	// 加载时不绘制 3D 世界
	GameViewportClient->bDisableWorldRendering = bEnabingLoadingScreen;

	// 加载界面显示时，确保优先流式加载关卡
	if (UWorld* ViewportWorld = GameViewportClient->GetWorld())
	{
		if (AWorldSettings* WorldSettings = ViewportWorld->GetWorldSettings(false, false))
		{
			WorldSettings->bHighPriorityLoadingLocal = bEnabingLoadingScreen;
		}
	}

	if (bEnabingLoadingScreen)
	{
		// 加载界面可见时设置心跳超时倍率
		const double HangDurationMultiplier = GetDefault<UCommonLoadingScreenSettings>()->LoadingScreenHangDurationMultiplier;
		FThreadHeartBeat::Get().SetDurationMultiplier(HangDurationMultiplier);

		// 加载界面显示时不报告卡顿
		FGameThreadHitchHeartBeat::Get().SuspendHeartBeat();
	}
	else
	{
		// 隐藏加载界面时恢复挂起检测器超时
		FThreadHeartBeat::Get().SetDurationMultiplier(1.0);

		// 加载界面关闭后恢复卡顿报告
		FGameThreadHitchHeartBeat::Get().ResumeHeartBeat();
	}
}

void ULoadingScreenManager::ChangePerformanceSettingsForBlackScreen(bool bEnablingBlackScreen)
{
	UGameInstance* LocalGameInstance = GetGameInstance();
	UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();

	FShaderPipelineCache::SetBatchMode(bEnablingBlackScreen ? FShaderPipelineCache::BatchMode::Fast : FShaderPipelineCache::BatchMode::Background);

	// 黑屏显示时不绘制 3D 世界
	GameViewportClient->bDisableWorldRendering = bEnablingBlackScreen;

	// 黑屏显示时，确保优先流式加载关卡
	if (UWorld* ViewportWorld = GameViewportClient->GetWorld())
	{
		if (AWorldSettings* WorldSettings = ViewportWorld->GetWorldSettings(false, false))
		{
			WorldSettings->bHighPriorityLoadingLocal = bEnablingBlackScreen;
		}
	}

	if (bEnablingBlackScreen)
	{
		const double HangDurationMultiplier = GetDefault<UCommonLoadingScreenSettings>()->BlackScreenHangDurationMultiplier;
		FThreadHeartBeat::Get().SetDurationMultiplier(HangDurationMultiplier);

		FGameThreadHitchHeartBeat::Get().SuspendHeartBeat();
	}
	else
	{
		FThreadHeartBeat::Get().SetDurationMultiplier(1.0);
		FGameThreadHitchHeartBeat::Get().ResumeHeartBeat();
	}
}



