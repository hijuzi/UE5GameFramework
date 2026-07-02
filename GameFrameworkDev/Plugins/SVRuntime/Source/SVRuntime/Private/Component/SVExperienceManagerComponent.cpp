// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/SVExperienceManagerComponent.h"
#include "Assets/SVExperienceDefinition.h"
#include "ControlFlowManager.h"
#include "LevelLoading/LevelLoadingManager.h"
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
	if (ULevelLoadingManager* LevelMgr = GetLevelLoadingManager())
	{
		LevelMgr->LevelLoadingScreenVisibilityChanged.Remove(BeginPlayWait_LSHandle);
	}
}

void USVExperienceManagerComponent::RegisterBeginPlayWaitScreenEvents()
{
	if (ULevelLoadingManager* LevelMgr = GetLevelLoadingManager())
	{
		BeginPlayWait_LSHandle = LevelMgr->LevelLoadingScreenVisibilityChanged.AddUObject(
			this, &ThisClass::OnBeginPlayWaitScreenHidden);
	}
}

ULevelLoadingManager* USVExperienceManagerComponent::GetLevelLoadingManager() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetGameInstance()->GetSubsystem<ULevelLoadingManager>();
	}
	return nullptr;
}

bool USVExperienceManagerComponent::IsLevelLoadingScreenDisplayed() const
{
	if (const ULevelLoadingManager* LevelMgr = GetLevelLoadingManager())
	{
		return LevelMgr->IsLevelLoadingScreenPersistent();
	}
	return false;
}

void USVExperienceManagerComponent::WaitForScreensHiddenThenStartExperience()
{
	if (!IsLevelLoadingScreenDisplayed())
	{
		// 无界面显示，直接启动 Experience
		UE_LOG(LogSVExperience, Log, TEXT("[BeginPlay] 当前无 LevelLoadingScreen 显示，直接启动 Experience 流程"));
		UnregisterBeginPlayWaitScreenEvents();
		OnExperienceLoaded();
	}
	else
	{
		// 有界面显示，注册监听等待隐藏
		UE_LOG(LogSVExperience, Log, TEXT("[BeginPlay] 检测到 LevelLoadingScreen 正在显示，注册 VisibilityChanged 委托等待隐藏后启动..."));
		UnregisterBeginPlayWaitScreenEvents();
		RegisterBeginPlayWaitScreenEvents();
	}
}

void USVExperienceManagerComponent::OnBeginPlayWaitScreenHidden(bool bIsVisible)
{
	if (!bIsVisible)
	{
		bBeginPlayWaitCompleted = true;
		UE_LOG(LogSVExperience, Log, TEXT("[BeginPlay] LevelLoadingScreen 已隐藏，解除委托并启动 Experience 流程"));
		UnregisterBeginPlayWaitScreenEvents();
		OnExperienceLoaded();
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
					SubFlow->ContinueFlow();
					break;
				}
			});
	}
}
