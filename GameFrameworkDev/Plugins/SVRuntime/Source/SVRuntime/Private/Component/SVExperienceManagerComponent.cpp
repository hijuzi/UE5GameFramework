// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/SVExperienceManagerComponent.h"
#include "SVWorldSettings.h"
#include "Assets/SVExperienceDefinition.h"
#include "ControlFlowManager.h"
#include "LoadingScreenManager.h"
#include "Core/PrimaryGameUILayout.h"
#include "GameplayTags/CommonGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogSVExperience, Log, All);

#include UE_INLINE_GENERATED_CPP_BY_NAME(SVExperienceManagerComponent)

USVExperienceManagerComponent::USVExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USVExperienceManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	OnExperienceLoaded();
}

void USVExperienceManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
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

void USVExperienceManagerComponent::OnExperienceLoaded()
{
	// 从 WorldSettings 获取 ExperienceDefinition，读取 UI 类配置
	if (const UWorld* World = GetWorld())
	{
		if (const ASVWorldSettings* WorldSettings = Cast<ASVWorldSettings>(World->GetWorldSettings()))
		{
			if (UClass* ExperienceClass = WorldSettings->GetDefaultGameplayExperienceSoftPtr().LoadSynchronous())
			{
			if (const USVBaseExperienceDefinition* ExperienceDef = GetDefault<USVBaseExperienceDefinition>(ExperienceClass))
			{
				MainScreenClass = ExperienceDef->MainScreenClass;

				// Login 类型的 Experience 额外提供 PressStartScreenClass 和 CompilingShadersScreenClass
				if (const USVLoginExperienceDefinition* LoginExperienceDef = Cast<USVLoginExperienceDefinition>(ExperienceDef))
				{
					CompilingShadersScreenClass = LoginExperienceDef->CompilingShadersScreenClass;
					PressStartScreenClass = LoginExperienceDef->PressStartScreenClass;
				}
			}
			}
		}
	}
	bShouldShowLoadingScreen = true;
	FControlFlow& Flow = FControlFlowStatics::Create(this, TEXT("ExperienceFlow"))
		.QueueStep(TEXT("Wait For Any Screen Hidden"), this, &ThisClass::FlowStep_WaitForAnyScreenHidden)
		.QueueStep(TEXT("Try Show Compiling Shaders Screen"), this, &ThisClass::FlowStep_TryShowCompilingShadersScreen)
		.QueueStep(TEXT("Try Show Press Start Screen"), this, &ThisClass::FlowStep_TryShowPressStartScreen)
		.QueueStep(TEXT("Try Show Main Screen"), this, &ThisClass::FlowStep_TryShowMainScreen);

	Flow.ExecuteFlow();

	FrontEndFlow = Flow.AsShared();
}

void USVExperienceManagerComponent::FlowStep_WaitForAnyScreenHidden(FControlFlowNodeRef SubFlow)
{
	if (UWorld* World = GetWorld())
	{
		if (ULoadingScreenManager* LSM = World->GetGameInstance()->GetSubsystem<ULoadingScreenManager>())
		{
			if (!LSM->GetAnyScreenDisplayStatus())
			{
				UE_LOG(LogSVExperience, Log, TEXT("[WaitForAnyScreenHidden] 当前无界面显示，直接进入下一步"));
				SubFlow->ContinueFlow();
				return;
			}

			UE_LOG(LogSVExperience, Log, TEXT("[WaitForAnyScreenHidden] 检测到界面正在显示，绑定 VisibilityChanged 委托等待隐藏..."));
			TSharedPtr<FDelegateHandle> LoadingScreenHandle = MakeShared<FDelegateHandle>();
			TSharedPtr<FDelegateHandle> BlackScreenHandle = MakeShared<FDelegateHandle>();
			TSharedPtr<bool> bCompleted = MakeShared<bool>(false);

			auto CheckAndContinue = [this, SubFlow, LSM, LoadingScreenHandle, BlackScreenHandle, bCompleted]()
			{
				if (*bCompleted)
				{
					return;
				}

				if (!LSM->GetAnyScreenDisplayStatus())
				{
					*bCompleted = true;
					UE_LOG(LogSVExperience, Log, TEXT("[WaitForAnyScreenHidden] 界面已隐藏，解除委托并进入下一步"));
					LSM->OnLoadingScreenVisibilityChangedDelegate().Remove(*LoadingScreenHandle);
					LSM->OnBlackScreenVisibilityChangedDelegate().Remove(*BlackScreenHandle);
					SubFlow->ContinueFlow();
				}
			};

			*LoadingScreenHandle = LSM->OnLoadingScreenVisibilityChangedDelegate().AddWeakLambda(this, [CheckAndContinue](bool bIsVisible)
			{
				CheckAndContinue();
			});

			*BlackScreenHandle = LSM->OnBlackScreenVisibilityChangedDelegate().AddWeakLambda(this, [CheckAndContinue](bool bIsVisible)
			{
				CheckAndContinue();
			});

			return;
		}
	}

	// 无法获取 LSM，直接继续
	UE_LOG(LogSVExperience, Warning, TEXT("[WaitForAnyScreenHidden] 无法获取 LoadingScreenManager，直接进入下一步"));
	SubFlow->ContinueFlow();
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
