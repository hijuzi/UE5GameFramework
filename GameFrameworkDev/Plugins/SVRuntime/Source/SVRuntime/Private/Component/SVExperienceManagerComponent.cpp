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
				if (const USVExperienceDefinition* ExperienceDef = GetDefault<USVExperienceDefinition>(ExperienceClass))
				{
					PressStartScreenClass = ExperienceDef->PressStartScreenClass;
					MainScreenClass = ExperienceDef->MainScreenClass;
				}
			}
		}
	}

	FControlFlow& Flow = FControlFlowStatics::Create(this, TEXT("ExperienceFlow"))
		.QueueStep(TEXT("Try Show Press Start Screen"), this, &ThisClass::FlowStep_TryShowPressStartScreen)
		.QueueStep(TEXT("Try Show Main Screen"), this, &ThisClass::FlowStep_TryShowMainScreen);

	Flow.ExecuteFlow();

	FrontEndFlow = Flow.AsShared();
}

void USVExperienceManagerComponent::FlowStep_TryShowPressStartScreen(FControlFlowNodeRef SubFlow)
{
	if (PressStartScreenClass.IsNull())
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
			PressStartScreenClass,
			[this, SubFlow](EAsyncWidgetLayerState State, UCommonActivatableWidget* Screen)
			{
				switch (State)
				{
				case EAsyncWidgetLayerState::AfterPush:
					bShouldShowLoadingScreen = false;
					Screen->OnDeactivated().AddWeakLambda(this, [this, SubFlow]()
					{
						SubFlow->ContinueFlow();
					});
					break;
				case EAsyncWidgetLayerState::Canceled:
					bShouldShowLoadingScreen = false;
					SubFlow->ContinueFlow();
					break;
				}
			});
	}
}

void USVExperienceManagerComponent::FlowStep_TryShowMainScreen(FControlFlowNodeRef SubFlow)
{
	if (MainScreenClass.IsNull())
	{
		bShouldShowLoadingScreen = false;
		SubFlow->ContinueFlow();
		return;
	}

	if (UPrimaryGameUILayout* RootLayout = UPrimaryGameUILayout::GetPrimaryGameLayoutForPrimaryPlayer(this))
	{
		constexpr bool bSuspendInputUntilComplete = true;
		RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(
			CommonGameplayTags::TAG_GAMEUI_LAYER_FULLSCREENMENU,
			bSuspendInputUntilComplete,
			MainScreenClass,
			[this, SubFlow](EAsyncWidgetLayerState State, UCommonActivatableWidget* Screen)
			{
				switch (State)
				{
				case EAsyncWidgetLayerState::AfterPush:
					bShouldShowLoadingScreen = false;
					SubFlow->ContinueFlow();
					return;
				case EAsyncWidgetLayerState::Canceled:
					bShouldShowLoadingScreen = false;
					SubFlow->ContinueFlow();
					return;
				}
			});
	}
}
