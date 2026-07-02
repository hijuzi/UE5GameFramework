// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "LoadingScreenInterface.generated.h"

// 前向声明，避免循环依赖
enum class ELoadingAnimationType : uint8;
enum class ELoadingAnimationMode : uint8;
enum class ELoadingScreenContentType : uint8;

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
	static LOADINGSCREENSYSTEM_API bool ShouldShowLevelLoadingScreen(UObject* TestObject, FString& OutReason);

	virtual bool ShouldShowLevelLoadingScreen(FString& OutReason) const
	{
		return false;
	}
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
