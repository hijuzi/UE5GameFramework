// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LoadingProcessInterface.h"
#include "UObject/Object.h"

#include "LoadingProcessTask.generated.h"

#define UE_API COMMONLOADINGSCREEN_API

struct FFrame;

UCLASS(MinimalAPI, BlueprintType)
class ULoadingProcessTask : public UObject, public ILoadingProcessInterface
{
	GENERATED_BODY()
	
public:
	/** 创建一个加载处理任务并注册到 LoadingScreenManager，使加载界面显示 */
	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject"))
	static UE_API ULoadingProcessTask* CreateLoadingScreenProcessTask(UObject* WorldContextObject, const FString& ShowLoadingScreenReason);

public:
	ULoadingProcessTask() { }

	/** 标记任务完成并从 LoadingScreenManager 中取消注册，使加载界面可以被关闭 */
	UFUNCTION(BlueprintCallable)
	UE_API void CompleteLoadingScreenProcessTask();

	/** 设置加载界面显示的原因说明 */
	UFUNCTION(BlueprintCallable)
	UE_API void SetShowLoadingScreenReason(const FString& InReason);

	UE_API virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;
	
private:
	/** 加载界面显示的原因描述 */
	FString Reason;

	/** 任务是否已完成（完成后不再需要保持加载界面） */
	bool bIsComplete = false;
};

#undef UE_API
