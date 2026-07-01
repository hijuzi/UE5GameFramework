// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/SoftObjectPath.h"

#include "LoadingScreenInterface.generated.h"

#define UE_API LOADINGSCREENSYSTEM_API

// 前向声明，避免循环依赖
enum class ELoadingAnimationType : uint8;
enum class ELoadingAnimationMode : uint8;
enum class ELoadingScreenContentType : uint8;

/**
 * 关卡加载界面覆盖参数结构体
 */
USTRUCT(BlueprintType)
struct LOADINGSCREENSYSTEM_API FLevelLoadingScreenOverrideConfig
{
	GENERATED_BODY()

	// ---- 覆盖开关 ----

	/** 是否覆盖 Content 参数 */
	UPROPERTY()
	bool bOverrideContent = false;

	// ---- Content 参数 ----

	/** 关卡加载界面内容类型 */
	UPROPERTY()
	uint8 ContentType = 0;

	/** 图片背景资产（ContentType 为 Image 时生效） */
	UPROPERTY()
	FSoftObjectPath ImageBackground;

	/** 视频路径（ContentType 为 Video 时生效） */
	UPROPERTY()
	FString VideoPath;
};

/** 用于那些可能触发加载、从而需要显示关卡加载界面的对象的接口 */
UINTERFACE(MinimalAPI, BlueprintType)
class ULevelLoadingScreenInterface : public UInterface
{
	GENERATED_BODY()
};

class ILevelLoadingScreenInterface
{
	GENERATED_BODY()

public:
	// 检查此对象是否实现了该接口，如果是则询问是否应显示关卡加载界面
	static UE_API bool ShouldShowLevelLoadingScreen(UObject* TestObject, FString& OutReason);

	virtual bool ShouldShowLevelLoadingScreen(FString& OutReason) const
	{
		return false;
	}

	/** 获取关卡加载界面覆盖参数。默认返回空（无覆盖）。 */
	virtual void GetLevelLoadingScreenOverrideConfig(FLevelLoadingScreenOverrideConfig& OutConfig) const
	{
		OutConfig = FLevelLoadingScreenOverrideConfig();
	}

	/** 
	 * 静态方法：从 WorldContext 中查找 ILevelLoadingScreenInterface 实现者，收集关卡加载界面覆盖参数。
	 * 优先使用第一个返回 bOverrideContent 为 true 的配置。
	 */
	static UE_API FLevelLoadingScreenOverrideConfig GetLevelLoadingScreenOverrideConfig(const UObject* WorldContextObject);
};

// ----------------------------------------------------------------------------
// 黑屏加载接口
// ----------------------------------------------------------------------------

/** 用于需要触发黑屏加载界面的对象的接口 */
UINTERFACE(MinimalAPI, BlueprintType)
class UBlackLoadingProcessInterface : public UInterface
{
	GENERATED_BODY()
};

class IBlackLoadingProcessInterface
{
	GENERATED_BODY()

public:
	// 检查此对象是否实现了该接口，如果是则询问是否应显示黑屏加载界面
	static bool ShouldShowLoadingScreen(UObject* TestObject, FString& OutReason)
	{
		if (TestObject != nullptr)
		{
			if (IBlackLoadingProcessInterface* LoadObserver = Cast<IBlackLoadingProcessInterface>(TestObject))
			{
				FString ObserverReason;
				if (LoadObserver->ShouldShowLoadingScreen(/*out*/ ObserverReason))
				{
					if (ensureMsgf(!ObserverReason.IsEmpty(), TEXT("%s failed to set a reason why it wants to show the loading screen"), *GetPathNameSafe(TestObject)))
					{
						OutReason = ObserverReason;
					}
					return true;
				}
			}
		}

		return false;
	}

	virtual bool ShouldShowLoadingScreen(FString& OutReason) const
	{
		return false;
	}
};

#undef UE_API
