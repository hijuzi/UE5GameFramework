// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/SVExperienceManagerComponent.h"
#include "SVWorldSettings.h"
#include "Assets/SVExperienceDefinition.h"
#include "ControlFlowManager.h"
#include "LoadingScreenManager.h"
#include "Core/PrimaryGameUILayout.h"
#include "GameplayTags/CommonGameplayTags.h"
#include "PSOCacheManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSVExperience, Log, All);

#include UE_INLINE_GENERATED_CPP_BY_NAME(SVExperienceManagerComponent)

USVExperienceManagerComponent::USVExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USVExperienceManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	WaitForScreensHiddenThenStartExperience();
}

void USVExperienceManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	UnregisterBeginPlayWaitScreenEvents();
}

void USVExperienceManagerComponent::UnregisterBeginPlayWaitScreenEvents()
{
	if (ULoadingScreenManager* LSM = GetLoadingScreenManager())
	{
		LSM->OnLoadingScreenVisibilityChangedDelegate().Remove(BeginPlayWait_LSHandle);
		LSM->OnBlackScreenVisibilityChangedDelegate().Remove(BeginPlayWait_BSHandle);
	}
}

void USVExperienceManagerComponent::RegisterBeginPlayWaitScreenEvents()
{
	if (ULoadingScreenManager* LSM = GetLoadingScreenManager())
	{
		BeginPlayWait_LSHandle = LSM->OnLoadingScreenVisibilityChangedDelegate().AddUObject(
			this, &ThisClass::OnBeginPlayWaitScreenHidden);

		BeginPlayWait_BSHandle = LSM->OnBlackScreenVisibilityChangedDelegate().AddUObject(
			this, &ThisClass::OnBeginPlayWaitScreenHidden);
	}
}

bool USVExperienceManagerComponent::ShouldUseLoadingScreen() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const ASVWorldSettings* WorldSettings = Cast<ASVWorldSettings>(World->GetWorldSettings()))
		{
			return WorldSettings->bUseLoadingScreen;
		}
	}
	return false;
}

ULoadingScreenManager* USVExperienceManagerComponent::GetLoadingScreenManager() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetGameInstance()->GetSubsystem<ULoadingScreenManager>();
	}
	return nullptr;
}

void USVExperienceManagerComponent::WaitForScreensHiddenThenStartExperience()
{
	ULoadingScreenManager* LSM = GetLoadingScreenManager();
	if (LSM && !LSM->GetAnyScreenDisplayStatus())
	{
		// 无界面显示，直接启动 Experience
		UE_LOG(LogSVExperience, Log, TEXT("[BeginPlay] 当前无界面显示，直接启动 Experience 流程"));
		UnregisterBeginPlayWaitScreenEvents();
		OnExperienceLoaded();
	}
	else
	{
		// LSM为空或者有界面显示，注册监听等待隐藏
		UE_LOG(LogSVExperience, Log, TEXT("[BeginPlay] 检测到界面正在显示或者LSM为空，注册 VisibilityChanged 委托等待隐藏后启动..."));
		UnregisterBeginPlayWaitScreenEvents();
		RegisterBeginPlayWaitScreenEvents();
	}
}

void USVExperienceManagerComponent::OnBeginPlayWaitScreenHidden(bool bIsVisible)
{
	ULoadingScreenManager* LSM = GetLoadingScreenManager();
	if (!LSM)
	{
		return;
	}

	if (!LSM->GetAnyScreenDisplayStatus())
	{
		bBeginPlayWaitCompleted = true;
		UE_LOG(LogSVExperience, Log, TEXT("[BeginPlay] 界面已隐藏，解除委托并启动 Experience 流程"));
		UnregisterBeginPlayWaitScreenEvents();
		OnExperienceLoaded();
	}
}

bool USVExperienceManagerComponent::ShouldShowLoadingScreen(FString& OutReason) const
{
	// 如果关卡配置不使用 LoadingScreen，直接返回 false
	if (!ShouldUseLoadingScreen())
	{
		UE_LOG(LogSVExperience, Log, TEXT("[SVExperienceManagerComponent] ShouldUseLoadingScreen() == false，不请求显示。"));
		return false;
	}

	if (bShouldShowLoadingScreen)
	{
		OutReason = TEXT("Frontend Flow Pending...");

		if (FrontEndFlow.IsValid())
		{
			const TOptional<FString> StepDebugName = FrontEndFlow->GetCurrentStepDebugName();
			if (StepDebugName.IsSet())
			{
				OutReason = StepDebugName.GetValue();
			}
		}

		UE_LOG(LogSVExperience, Log, TEXT("[SVExperienceManagerComponent] bShouldShowLoadingScreen == true，请求显示。原因: %s"), *OutReason);
		return true;
	}

	UE_LOG(LogSVExperience, Log, TEXT("[SVExperienceManagerComponent] bShouldShowLoadingScreen == false（Flow 已推进），不请求显示。"));
	return false;
}

void USVExperienceManagerComponent::GetLoadingScreenOverrideConfig(FLoadingScreenOverrideConfig& OutConfig) const
{
	const USVBaseExperienceDefinition* ExperienceDef = USVBaseExperienceDefinition::GetCurrentExperienceDefinition(GetWorld());
	if (!ExperienceDef)
	{
		return;
	}

	// Timing 覆盖
	if (ExperienceDef->bOverrideLoadingScreenTiming)
	{
		OutConfig.bOverrideTiming   = true;
		OutConfig.LoadDuration      = ExperienceDef->LoadingScreenLoadDurationOverride;
		OutConfig.UnloadDuration    = ExperienceDef->LoadingScreenUnloadDurationOverride;
		OutConfig.AnimationType     = static_cast<uint8>(ExperienceDef->LoadingScreenAnimationTypeOverride);
		OutConfig.AnimationMode     = static_cast<uint8>(ExperienceDef->LoadingScreenAnimationModeOverride);
	}

	// Content 覆盖
	if (ExperienceDef->bOverrideLoadingScreenContent)
	{
		OutConfig.bOverrideContent  = true;
		OutConfig.ContentType       = static_cast<uint8>(ExperienceDef->LoadingScreenContentTypeOverride);
		OutConfig.ImageBackground   = ExperienceDef->LoadingScreenImageBackgroundOverride;
		OutConfig.VideoPath         = ExperienceDef->LoadingScreenVideoPathOverride;
	}
}

void USVExperienceManagerComponent::OnExperienceLoaded()
{
	// 从 WorldSettings 获取 ExperienceDefinition，读取 UI 类配置
	if (const USVBaseExperienceDefinition* ExperienceDef = USVBaseExperienceDefinition::GetCurrentExperienceDefinition(GetWorld()))
	{
		MainScreenClass = ExperienceDef->MainScreenClass;
	}

	// Login 类型的 Experience 额外提供 PressStartScreenClass 和 CompilingShadersScreenClass
	if (const USVLoginExperienceDefinition* LoginExperienceDef = USVLoginExperienceDefinition::GetCurrentLoginExperienceDefinition(GetWorld()))
	{
		CompilingShadersScreenClass = LoginExperienceDef->CompilingShadersScreenClass;
		bForceShowCompilingShadersScreen = LoginExperienceDef->bForceShowCompilingShadersScreen;
		bEnableCompilingShadersInEditor = LoginExperienceDef->bEnableCompilingShadersInEditor;
		PressStartScreenClass = LoginExperienceDef->PressStartScreenClass;
	}
	bShouldShowLoadingScreen = true;
	FControlFlow& Flow = FControlFlowStatics::Create(this, TEXT("ExperienceFlow"))
		.QueueStep(TEXT("Try Show Compiling Shaders Screen"), this, &ThisClass::FlowStep_TryShowCompilingShadersScreen)
		.QueueStep(TEXT("Try Show Press Start Screen"), this, &ThisClass::FlowStep_TryShowPressStartScreen)
		.QueueStep(TEXT("Try Show Main Screen"), this, &ThisClass::FlowStep_TryShowMainScreen);

	Flow.ExecuteFlow();

	FrontEndFlow = Flow.AsShared();
}



void USVExperienceManagerComponent::FlowStep_TryShowCompilingShadersScreen(FControlFlowNodeRef SubFlow)
{
#if WITH_EDITOR
	// 编辑器下：如果强制开启且编辑器也启用，直接显示着色器界面
	if (bForceShowCompilingShadersScreen && bEnableCompilingShadersInEditor)
	{
		UE_LOG(LogSVExperience, Log, TEXT("编辑器模式：强制开启着色器编译界面"));
		TryPushWidgetToLayer(SubFlow, CompilingShadersScreenClass, true);
		return;
	}
#endif

	// 检查 PSO 是否已全部预编译完成，如果已完成则跳过着色器编译界面
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UPSOCacheManager* PSOCacheMgr = GameInstance->GetSubsystem<UPSOCacheManager>())
			{
				const int32 Remaining = PSOCacheMgr->GetPrecompilesRemaining();
				if (Remaining == 0)
				{
					// PSO 已完成，检查是否强制显示着色器界面
					if (bForceShowCompilingShadersScreen)
					{
						UE_LOG(LogSVExperience, Log, TEXT("PSO 预编译已完成，但强制开启着色器编译界面"));
						TryPushWidgetToLayer(SubFlow, CompilingShadersScreenClass, true);
						return;
					}
					UE_LOG(LogSVExperience, Log, TEXT("PSO 预编译已完成，跳过着色器编译界面"));
					SubFlow->ContinueFlow();
					return;
				}
				UE_LOG(LogSVExperience, Log, TEXT("PSO 剩余 %d 个待预编译，显示着色器编译界面"), Remaining);
			}
		}
	}
	TryPushWidgetToLayer(SubFlow, CompilingShadersScreenClass, true);
}

void USVExperienceManagerComponent::TryPushWidgetToLayer(FControlFlowNodeRef SubFlow, const TSoftClassPtr<UCommonActivatableWidget>& ScreenClass, bool bWaitForDeactivation)
{
	if (ScreenClass.IsNull())
	{
		SubFlow->ContinueFlow();
		return;
	}

	if (UPrimaryGameUILayout* RootLayout = UPrimaryGameUILayout::GetPrimaryGameLayoutForPrimaryPlayer(this))
	{
		constexpr bool bSuspendInputUntilComplete = true;
		RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(
			CommonGameplayTags::TAG_GAMEUI_LAYER_FULLSCREENMENU,
			bSuspendInputUntilComplete,
			ScreenClass,
			[this, SubFlow, bWaitForDeactivation](EAsyncWidgetLayerState State, UCommonActivatableWidget* Screen)
			{
				switch (State)
				{
				case EAsyncWidgetLayerState::AfterPush:
					bShouldShowLoadingScreen = false;
					if (bWaitForDeactivation)
					{
						Screen->OnDeactivated().AddWeakLambda(this, [this, SubFlow]()
						{
							SubFlow->ContinueFlow();
						});
					}
					else
					{
						SubFlow->ContinueFlow();
					}
					break;
				case EAsyncWidgetLayerState::Canceled:
					bShouldShowLoadingScreen = false;
					SubFlow->ContinueFlow();
					break;
				}
			});
	}
}
