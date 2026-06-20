// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "LoadingProcessInterface.generated.h"

#define UE_API COMMONLOADINGSCREEN_API

/** 用于那些可能触发加载、从而需要显示加载界面的对象的接口 */
UINTERFACE(MinimalAPI, BlueprintType)
class ULoadingProcessInterface : public UInterface
{
	GENERATED_BODY()
};

class ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	// 检查此对象是否实现了该接口，如果是则询问是否应显示加载界面
	static UE_API bool ShouldShowLoadingScreen(UObject* TestObject, FString& OutReason);

	virtual bool ShouldShowLoadingScreen(FString& OutReason) const
	{
		return false;
	}
};

#undef UE_API
