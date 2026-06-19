// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/SVExperienceManagerComponent.h"
#include "SVWorldSettings.h"
#include "Assets/SVExperienceDefinition.h"
#include "ControlFlowManager.h"
#include "Core/PrimaryGameUILayout.h"
#include "GameplayTags/CommonGameplayTags.h"

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

bool USVExperienceManagerComponent::ShouldShowLoadingScreen(FString& OutReason) const
{
	// 如果 Experience 配置不使用 LoadingScreen，直接返回 false
	if (!bUseLoadingScreen)
	{
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

		return true;
	}

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
				bUseLoadingScreen = ExperienceDef->bUseLoadingScreen;
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

	FControlFlow& Flow = FControlFlowStatics::Create(this, TEXT("ExperienceFlow"))
		.QueueStep(TEXT("Try Show Compiling Shaders Screen"), this, &ThisClass::FlowStep_TryShowCompilingShadersScreen)
		.QueueStep(TEXT("Try Show Press Start Screen"), this, &ThisClass::FlowStep_TryShowPressStartScreen)
		.QueueStep(TEXT("Try Show Main Screen"), this, &ThisClass::FlowStep_TryShowMainScreen);

	Flow.ExecuteFlow();

	FrontEndFlow = Flow.AsShared();
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
