// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackLoading/BlackLoadingManager.h"
#include "LoadingScreenInterface.h"
#include "BlackLoading/BlackLoadingScreenWidget.h"
#include "BlackLoading/BlackLoadingProcessTask.h"
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

// Profiling category
CSV_DEFINE_CATEGORY(BlackLoadingScreen, true);


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
	if (!IsBlackLoadingScreenAnimationPlaying() && !IsBlackLoadingScreenAnimationPlaying())
	{
		UpdateBlackLoadingScreen();
	}
	
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

void UBlackLoadingManager::OpenBlackLoadingScreen(const FString& Reason, bool bAutoClose)
{
	bAutoCloseBlackLoadingScreen = bAutoClose;

	if (BlackLoadingProcessTask)
	{
		// 已存在则重新注册（之前被 Close 时仅 UnregisterFromManager，UObject 仍存活）
		BlackLoadingProcessTask->RegisterWithManager(Reason);
	}
	else
	{
		BlackLoadingProcessTask = UBlackLoadingProcessTask::CreateBlackLoadingProcessTask(
			GetGameInstance(), Reason);
	}
	UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] 打开黑屏过渡界面: %s"), *Reason);
}

void UBlackLoadingManager::CloseBlackLoadingScreen(const FString& Reason)
{
	if (BlackLoadingProcessTask)
	{
		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] 关闭黑屏过渡界面: %s"), *Reason);
		BlackLoadingProcessTask->UnregisterFromManager(Reason);
	}
}

void UBlackLoadingManager::UpdateBlackLoadingScreen()
{
	const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();
	bool bLogLoadingScreenStatus = Settings->bLogLoadingScreenReasonEveryFrame;

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
		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] 显示状态: %d, 原因: %s"), bCurrentlyShowingBlackLoadingScreen ? 1 : 0, *DebugReasonForShowingOrHidingBlackLoadingScreen);
	}
}

bool UBlackLoadingManager::CheckForAnyNeedToShowBlackLoadingScreen()
{
	DebugReasonForShowingOrHidingBlackLoadingScreen = TEXT("Reason for Showing/Hiding LoadingScreen is unknown!");

	const UGameInstance* LocalGameInstance = GetGameInstance();
	const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();

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

	// 引擎仍在显示其加载界面时无法显示加载界面。
	if(IsShowingInitialBlackLoadingScreen())
	{
		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] InitialLoadingScreen 显示中，等待其完成"));
		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] %s"), *DebugReasonForShowingOrHidingBlackLoadingScreen);
	}
	else
	{
		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] 开始显示黑屏加载界面"));
		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] %s"), *DebugReasonForShowingOrHidingBlackLoadingScreen);

		UGameInstance* LocalGameInstance = GetGameInstance();
		const ULoadingScreenSettings* Settings = GetDefault<ULoadingScreenSettings>();

		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] 显示黑屏加载界面，原因: %s"), *DebugReasonForShowingOrHidingBlackLoadingScreen);

		// 创建加载界面控件 — 优先使用配置的 UMG 子类，留空时回退到 UBlackLoadingScreenWidget 基类
		TSubclassOf<UUserWidget> LoadingScreenWidgetClass = Settings->BlackLoadingScreenWidget.TryLoadClass<UUserWidget>();
		if (!LoadingScreenWidgetClass)
		{
			UE_LOG(LogBlackLoading, Warning, TEXT("[黑屏加载界面] 未配置 Widget 类，使用默认 Widget"));
			LoadingScreenWidgetClass = UBlackLoadingScreenWidget::StaticClass();
		}

		UUserWidget* UserWidget = UUserWidget::CreateWidgetInstance(*LocalGameInstance, LoadingScreenWidgetClass, NAME_None);
		if (UBlackLoadingScreenWidget* LoadingScreenWidgetInstance = Cast<UBlackLoadingScreenWidget>(UserWidget))
		{
			UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] Widget 创建成功"));

			TimeBlackLoadingScreenShown = FPlatformTime::Seconds();

			CSV_EVENT(BlackLoadingScreen, TEXT("Show"));
			
			bCurrentlyShowingBlackLoadingScreen = true;
			BlackLoadingScreenUserWidgetPtr = LoadingScreenWidgetInstance;
			BlackLoadingScreenWidget = UserWidget->TakeWidget();

			// 绑定动画完成回调
			LoadingScreenWidgetInstance->OnLoadAnimationCompleted.AddDynamic(this, &UBlackLoadingManager::HandleLoadingScreenLoadAnimationCompleted);
			LoadingScreenWidgetInstance->OnUnloadAnimationCompleted.AddDynamic(this, &UBlackLoadingManager::HandleLoadingScreenUnloadAnimationCompleted);
			
			LoadingScreenWidgetInstance->StartLoadAnimation();
			
			// 添加到视口，高 ZOrder 确保在最上层
			UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();
			GameViewportClient->AddViewportWidgetContent(BlackLoadingScreenWidget.ToSharedRef(), Settings->BlackLoadingScreenZOrder);

			// 拦截输入
			StartBlockingInput();

			BlackLoadingScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ true);

			if (!GIsEditor || Settings->ForceTickLoadingScreenEvenInEditor)
			{
				// Tick Slate 以确保加载界面立即显示
				FSlateApplication::Get().Tick();
			}
		}
		else
		{
			UE_LOG(LogBlackLoading, Error, TEXT("[黑屏加载界面] Widget 实例创建失败"));
		}
	}
}

void UBlackLoadingManager::HideBlackLoadingScreen()
{
	if (!bCurrentlyShowingBlackLoadingScreen)
	{
		return;
	}

	if (IsShowingInitialBlackLoadingScreen())
	{
		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] InitialLoadingScreen 显示中，等待隐藏"));
		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] %s"), *DebugReasonForShowingOrHidingBlackLoadingScreen);
	}
	else
	{
		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] 开始隐藏黑屏加载界面"));
		UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] %s"), *DebugReasonForShowingOrHidingBlackLoadingScreen);

		if (BlackLoadingScreenUserWidgetPtr)
		{
			BlackLoadingScreenUserWidgetPtr->StartUnloadAnimation();
		}
		else
		{
			FinishBlackLoadingScreenCleanup();
		}
	}
}

void UBlackLoadingManager::FinishBlackLoadingScreenCleanup()
{
	StopBlockingInput();

	UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] 最终清理完成，原因: %s"), *DebugReasonForShowingOrHidingBlackLoadingScreen);

	RemoveBlackLoadingScreenWidgetFromViewport();

	BlackLoadingScreenUserWidgetPtr = nullptr;

	BlackLoadingScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ false);

	CSV_EVENT(BlackLoadingScreen, TEXT("Hide"));

	const double BlackLoadingScreenDuration = FPlatformTime::Seconds() - TimeBlackLoadingScreenShown;
	UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] 可见时长: %.2f 秒"), BlackLoadingScreenDuration);

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
		InputPreProcessor = MakeShareable<FLoadingScreenInputPreProcessor>(new FLoadingScreenInputPreProcessor());
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

void UBlackLoadingManager::HandleLoadingScreenLoadAnimationCompleted()
{
	UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] 淡入动画完成"));

	// 自动关闭模式下，动画完成后反注册任务，使后续 Tick 自然关闭黑屏
	if (bAutoCloseBlackLoadingScreen && BlackLoadingProcessTask)
	{
		CloseBlackLoadingScreen(TEXT("Load animation completed (auto close)"));
		bAutoCloseBlackLoadingScreen = false;
	}
}

void UBlackLoadingManager::HandleLoadingScreenUnloadAnimationCompleted()
{
	UE_LOG(LogBlackLoading, Log, TEXT("[黑屏加载界面] 淡出动画完成"));
	FinishBlackLoadingScreenCleanup();
}

bool UBlackLoadingManager::IsBlackLoadingScreenAnimationPlaying() const
{
	if (BlackLoadingScreenUserWidgetPtr)
	{
		return BlackLoadingScreenUserWidgetPtr->IsScreenAnimationPlaying();
	}
	return false;
}




