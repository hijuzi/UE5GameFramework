// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackLoading/BlackLoadingManager.h"
#include "LoadingScreenInterface.h"
#include "BlackLoading/BlackLoadingScreenWidget.h"
#include "LoadingScreenSettings.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/CommandLine.h"
#include "PreLoadScreenManager.h"
#include "Blueprint/UserWidget.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BlackLoadingManager)

#include "LogLoadingScreenSystem.h"
DEFINE_LOG_CATEGORY(LogBlackLoading);

// Profiling category
CSV_DEFINE_CATEGORY(BlackLoadingScreen, true);

//////////////////////////////////////////////////////////////////////

namespace BlackLoadingCVars
{
	static float HoldLoadingScreenAdditionalSecs = 2.0f;
	static FAutoConsoleVariableRef CVarHoldLoadingScreenUpAtLeastThisLongInSecs(
		TEXT("BlackLoading.HoldLoadingScreenAdditionalSecs"),
		HoldLoadingScreenAdditionalSecs,
		TEXT("How long to hold the loading screen up after other loading finishes (in seconds) to try to give texture streaming a chance to avoid blurriness"),
		ECVF_Default | ECVF_Preview);

	static bool LogLoadingScreenReasonEveryFrame = false;
	static FAutoConsoleVariableRef CVarLogLoadingScreenReasonEveryFrame(
		TEXT("BlackLoading.LogLoadingScreenReasonEveryFrame"),
		LogLoadingScreenReasonEveryFrame,
		TEXT("When true, the reason the loading screen is shown or hidden will be printed to the log every frame."),
		ECVF_Default);

	static bool ForceLoadingScreenVisible = false;
	static FAutoConsoleVariableRef CVarForceLoadingScreenVisible(
		TEXT("BlackLoading.AlwaysShow"),
		ForceLoadingScreenVisible,
		TEXT("Force the loading screen to show."),
		ECVF_Default);
}

//////////////////////////////////////////////////////////////////////
// FBlackLoadingScreenInputPreProcessor

// 输入拦截器：加载界面显示期间吃掉所有输入
class FBlackLoadingScreenInputPreProcessor : public IInputProcessor
{
public:
	FBlackLoadingScreenInputPreProcessor() {}
	virtual ~FBlackLoadingScreenInputPreProcessor() {}

	bool CanEatInput() const
	{
		return !GIsEditor;
	}

	//~ IInputProcessor interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return CanEatInput(); }
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return CanEatInput(); }
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override { return CanEatInput(); }
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override { return CanEatInput(); }
	virtual bool HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& MotionEvent) override { return CanEatInput(); }
	//~ End of IInputProcessor interface
};

//////////////////////////////////////////////////////////////////////
// UBlackLoadingManager

void UBlackLoadingManager::Initialize(FSubsystemCollectionBase& Collection)
{
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
}

void UBlackLoadingManager::Deinitialize()
{
	StopBlockingInput();
	RemoveBlackLoadingScreenWidgetFromViewport();

	// 移除 Ticker
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
}

bool UBlackLoadingManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// 仅客户端需要加载界面
	const UGameInstance* GameInstance = CastChecked<UGameInstance>(Outer);
	const bool bIsServerWorld = GameInstance->IsDedicatedServerInstance();
	return !bIsServerWorld;
}

bool UBlackLoadingManager::Tick(float DeltaTime)
{
	UpdateBlackLoadingScreen();

	TimeUntilNextLogHeartbeatSeconds = FMath::Max(TimeUntilNextLogHeartbeatSeconds - DeltaTime, 0.0);

	return true; // 保持 Ticker 持续运行
}

void UBlackLoadingManager::RegisterBlackLoadingProcessor(TScriptInterface<IBlackLoadingProcessInterface> Interface)
{
	ExternalBlackLoadingProcessors.Add(Interface.GetObject());
}

void UBlackLoadingManager::UnregisterBlackLoadingProcessor(TScriptInterface<IBlackLoadingProcessInterface> Interface)
{
	ExternalBlackLoadingProcessors.Remove(Interface.GetObject());
}

void UBlackLoadingManager::UpdateBlackLoadingScreen()
{
	bool bLogLoadingScreenStatus = BlackLoadingCVars::LogLoadingScreenReasonEveryFrame;

	if (ShouldShowBlackLoadingScreen())
	{
		ShowBlackLoadingScreen();
	}
	else
	{
		HideBlackLoadingScreen();
	}

	if (bLogLoadingScreenStatus)
	{
		UE_LOG(LogBlackLoading, Log, TEXT("Loading screen showing: %d. Reason: %s"), bCurrentlyShowingBlackLoadingScreen ? 1 : 0, *DebugReasonForShowingOrHidingBlackLoadingScreen);
	}
}

bool UBlackLoadingManager::CheckForAnyNeedToShowBlackLoadingScreen()
{
	DebugReasonForShowingOrHidingBlackLoadingScreen = TEXT("Reason for Showing/Hiding LoadingScreen is unknown!");

	const UGameInstance* LocalGameInstance = GetGameInstance();

	if (BlackLoadingCVars::ForceLoadingScreenVisible)
	{
		DebugReasonForShowingOrHidingBlackLoadingScreen = FString(TEXT("BlackLoading.AlwaysShow is true"));
		return true;
	}

	const FWorldContext* Context = LocalGameInstance->GetWorldContext();
	if (Context == nullptr)
	{
		DebugReasonForShowingOrHidingBlackLoadingScreen = FString(TEXT("The game instance has a null WorldContext"));
		return true;
	}

	UWorld* World = Context->World();
	if (World == nullptr)
	{
		DebugReasonForShowingOrHidingBlackLoadingScreen = FString(TEXT("We have no world (FWorldContext's World() is null)"));
		return true;
	}

	AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
	if (GameState == nullptr)
	{
		DebugReasonForShowingOrHidingBlackLoadingScreen = FString(TEXT("GameState hasn't yet replicated (it's null)"));
		return true;
	}

	// 询问外部注册的加载处理器
	for (const TWeakInterfacePtr<IBlackLoadingProcessInterface>& Processor : ExternalBlackLoadingProcessors)
	{
		if (IBlackLoadingProcessInterface::ShouldShowLoadingScreen(Processor.GetObject(), /*out*/ DebugReasonForShowingOrHidingBlackLoadingScreen))
		{
			return true;
		}
	}

	UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();
	const bool bIsInSplitscreen = GameViewportClient->GetCurrentSplitscreenConfiguration() != ESplitScreenType::None;

	if (bIsInSplitscreen && bMissingAnyLocalPC)
	{
		DebugReasonForShowingOrHidingBlackLoadingScreen = FString(TEXT("At least one missing local player controller in splitscreen"));
		return true;
	}

	if (!bIsInSplitscreen && !bFoundAnyLocalPC)
	{
		DebugReasonForShowingOrHidingBlackLoadingScreen = FString(TEXT("Need at least one local player controller"));
		return true;
	}

	DebugReasonForShowingOrHidingBlackLoadingScreen = TEXT("(nothing wants to show it anymore)");
	return false;
}

bool UBlackLoadingManager::ShouldShowBlackLoadingScreen()
{
	// 命令行强制关闭
#if !UE_BUILD_SHIPPING
	static bool bCmdLineNoLoadingScreen = FParse::Param(FCommandLine::Get(), TEXT("NoLoadingScreen"));
	if (bCmdLineNoLoadingScreen)
	{
		DebugReasonForShowingOrHidingBlackLoadingScreen = FString(TEXT("CommandLine has 'NoLoadingScreen'"));
		return false;
	}
#endif

	UGameInstance* LocalGameInstance = GetGameInstance();
	if (LocalGameInstance->GetGameViewportClient() == nullptr)
	{
		return false;
	}

	const bool bNeedToShowBlackLoadingScreen = CheckForAnyNeedToShowBlackLoadingScreen();

	return bNeedToShowBlackLoadingScreen;
}

bool UBlackLoadingManager::IsShowingInitialBlackLoadingScreen() const
{
	FPreLoadScreenManager* PreLoadScreenManager = FPreLoadScreenManager::Get();
	return (PreLoadScreenManager != nullptr) && PreLoadScreenManager->HasValidActivePreLoadScreen();
}

void UBlackLoadingManager::ShowBlackLoadingScreen()
{
	if (bCurrentlyShowingBlackLoadingScreen)
	{
		return;
	}

	TimeBlackLoadingScreenShown = FPlatformTime::Seconds();

	bCurrentlyShowingBlackLoadingScreen = true;

	CSV_EVENT(BlackLoadingScreen, TEXT("Show"));

	UE_LOG(LogBlackLoading, Log, TEXT("Showing black loading screen. Reason: %s"), *DebugReasonForShowingOrHidingBlackLoadingScreen);

	UGameInstance* LocalGameInstance = GetGameInstance();

	// 从项目设置中读取黑屏加载界面配置
	const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();

	// 创建加载界面 Widget：优先使用项目设置中的类，否则回退到 UBlackLoadingScreenWidget
	UClass* WidgetClass = Settings->BlackLoadingScreenWidget.TryLoadClass<UUserWidget>();
	if (!WidgetClass)
	{
		WidgetClass = UBlackLoadingScreenWidget::StaticClass();
	}

	if (UUserWidget* UserWidget = UUserWidget::CreateWidgetInstance(*LocalGameInstance, WidgetClass, NAME_None))
	{
		BlackLoadingScreenWidget = UserWidget->TakeWidget();
	}

	if (!BlackLoadingScreenWidget.IsValid())
	{
		UE_LOG(LogBlackLoading, Error, TEXT("BlackLoadingScreenWidget is still invalid after fallback, aborting."));
		return;
	}

	// 添加到视口，高 ZOrder 确保在最上层
	UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();
	GameViewportClient->AddViewportWidgetContent(BlackLoadingScreenWidget.ToSharedRef(), Settings->BlackLoadingScreenZOrder);

    // 拦截输入
	StartBlockingInput();

	BlackLoadingScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ true);

	// Tick Slate 确保加载界面立即显示
	FSlateApplication::Get().Tick();
}

void UBlackLoadingManager::HideBlackLoadingScreen()
{
	if (!bCurrentlyShowingBlackLoadingScreen)
	{
		return;
	}

	StopBlockingInput();

	UE_LOG(LogBlackLoading, Log, TEXT("Hiding black loading screen. Reason: %s"), *DebugReasonForShowingOrHidingBlackLoadingScreen);

	RemoveBlackLoadingScreenWidgetFromViewport();

	BlackLoadingScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ false);

	CSV_EVENT(BlackLoadingScreen, TEXT("Hide"));

	const double BlackLoadingScreenDuration = FPlatformTime::Seconds() - TimeBlackLoadingScreenShown;
	UE_LOG(LogBlackLoading, Log, TEXT("LoadingScreen was visible for %.2fs"), BlackLoadingScreenDuration);

	bCurrentlyShowingBlackLoadingScreen = false;
}

void UBlackLoadingManager::RemoveBlackLoadingScreenWidgetFromViewport()
{
	UGameInstance* LocalGameInstance = GetGameInstance();
	if (BlackLoadingScreenWidget.IsValid())
	{
		if (UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient())
		{
			GameViewportClient->RemoveViewportWidgetContent(BlackLoadingScreenWidget.ToSharedRef());
		}
		BlackLoadingScreenWidget.Reset();
	}
}

void UBlackLoadingManager::StartBlockingInput()
{
	if (!InputPreProcessor.IsValid())
	{
		InputPreProcessor = MakeShareable<FBlackLoadingScreenInputPreProcessor>(new FBlackLoadingScreenInputPreProcessor());
		FSlateApplication::Get().RegisterInputPreProcessor(InputPreProcessor, 0);
	}
}

void UBlackLoadingManager::StopBlockingInput()
{
	if (InputPreProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputPreProcessor);
		InputPreProcessor.Reset();
	}
}


