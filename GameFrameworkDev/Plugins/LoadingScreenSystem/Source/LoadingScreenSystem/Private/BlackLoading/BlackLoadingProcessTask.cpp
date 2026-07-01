// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackLoading/BlackLoadingProcessTask.h"
#include "BlackLoading/BlackLoadingManager.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BlackLoadingProcessTask)

/*static*/ UBlackLoadingProcessTask* UBlackLoadingProcessTask::CreateBlackLoadingProcessTask(UObject* WorldContextObject, const FString& ShowLoadingScreenReason)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UBlackLoadingManager* BlackLoadingManager = GameInstance ? GameInstance->GetSubsystem<UBlackLoadingManager>() : nullptr;

	if (BlackLoadingManager)
	{
		UBlackLoadingProcessTask* NewLoadingTask = NewObject<UBlackLoadingProcessTask>(BlackLoadingManager);
		NewLoadingTask->SetShowLoadingScreenReason(ShowLoadingScreenReason);

		BlackLoadingManager->RegisterBlackLoadingProcessor(NewLoadingTask);

		return NewLoadingTask;
	}

	return nullptr;
}

void UBlackLoadingProcessTask::CompleteLoadingScreenProcessTask()
{
	// 标记任务完成，ShouldShowLoadingScreen 将返回 false
	bIsComplete = true;

	// 从 BlackLoadingManager 中取消注册，允许黑屏加载界面关闭
	UBlackLoadingManager* BlackLoadingManager = Cast<UBlackLoadingManager>(GetOuter());
	if (BlackLoadingManager)
	{
		BlackLoadingManager->UnregisterBlackLoadingProcessor(this);
	}
}

void UBlackLoadingProcessTask::SetShowLoadingScreenReason(const FString& InReason)
{
	Reason = InReason;
}

bool UBlackLoadingProcessTask::ShouldShowLoadingScreen(FString& OutReason) const
{
	OutReason = Reason;

	// 任务完成时不再需要保持黑屏加载界面
	return !bIsComplete;
}
