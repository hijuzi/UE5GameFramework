// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LoadingScreenInterface.h"
#include "UObject/Object.h"

#include "BlackLoadingProcessTask.generated.h"

#define UE_API LOADINGSCREENSYSTEM_API

struct FFrame;

UCLASS(MinimalAPI, BlueprintType)
class UBlackLoadingProcessTask : public UObject, public IBlackLoadingProcessInterface
{
	GENERATED_BODY()

public:
	/** 创建一个黑屏加载处理任务并注册到 BlackLoadingManager，使黑屏加载界面显示 */
	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject"))
	static UE_API UBlackLoadingProcessTask* CreateBlackLoadingProcessTask(UObject* WorldContextObject, const FString& ShowLoadingScreenReason);

public:
	UBlackLoadingProcessTask() { }

	/** 标记任务完成并从 BlackLoadingManager 中取消注册，使黑屏加载界面可以被关闭 */
	UFUNCTION(BlueprintCallable)
	UE_API void CompleteLoadingScreenProcessTask();

	/** 设置黑屏加载界面显示的原因说明 */
	UFUNCTION(BlueprintCallable)
	UE_API void SetShowLoadingScreenReason(const FString& InReason);

	UE_API virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;

private:
	/** 黑屏加载界面显示的原因描述 */
	FString Reason;

	/** 任务是否已完成（完成后不再需要保持黑屏加载界面） */
	bool bIsComplete = false;
};

#undef UE_API
