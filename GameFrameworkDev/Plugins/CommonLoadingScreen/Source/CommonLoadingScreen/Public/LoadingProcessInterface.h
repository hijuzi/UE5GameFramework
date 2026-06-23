// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/SoftObjectPath.h"

#include "LoadingProcessInterface.generated.h"

#define UE_API COMMONLOADINGSCREEN_API

// 前向声明，避免循环依赖
enum class ELoadingAnimationType : uint8;
enum class ELoadingAnimationMode : uint8;
enum class ELoadingScreenContentType : uint8;

/**
 * 加载界面覆盖参数结构体，由实现了 ILoadingProcessInterface 的组件提供，
 * 用于在 Experience 级别覆盖全局 CommonLoadingScreenSettings。
 */
USTRUCT(BlueprintType)
struct COMMONLOADINGSCREEN_API FLoadingScreenOverrideConfig
{
	GENERATED_BODY()

	// ---- 覆盖开关 ----

	/** 是否覆盖 Timing 参数 */
	UPROPERTY()
	bool bOverrideTiming = false;

	/** 是否覆盖 Content 参数 */
	UPROPERTY()
	bool bOverrideContent = false;

	// ---- Timing 参数 ----

	/** 加载界面淡入动画时长（秒） */
	UPROPERTY()
	float LoadDuration = 0.5f;

	/** 加载界面淡出动画时长（秒） */
	UPROPERTY()
	float UnloadDuration = 0.5f;

	/** 加载界面动画过渡类型 */
	UPROPERTY()
	uint8 AnimationType = 0;

	/** 加载界面动画插值模式 */
	UPROPERTY()
	uint8 AnimationMode = 0;

	/** 加载界面最小显示时长（秒），用于防止进度过快时闪烁 */
	UPROPERTY()
	float MinimumLoadingScreenDisplayTime = 2.0f;

	// ---- Content 参数 ----

	/** 加载界面内容类型 */
	UPROPERTY()
	uint8 ContentType = 0;

	/** 图片背景资产（ContentType 为 Image 时生效） */
	UPROPERTY()
	FSoftObjectPath ImageBackground;

	/** 视频路径（ContentType 为 Video 时生效） */
	UPROPERTY()
	FString VideoPath;
};

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

	/** 获取加载界面覆盖参数。默认返回空（无覆盖）。 */
	virtual void GetLoadingScreenOverrideConfig(FLoadingScreenOverrideConfig& OutConfig) const
	{
		OutConfig = FLoadingScreenOverrideConfig();
	}

	/** 
	 * 静态方法：从 WorldContext 中查找 ILoadingProcessInterface 实现者，收集加载界面覆盖参数。
	 * 优先使用第一个返回 bOverrideTiming/bOverrideContent 为 true 的配置。
	 */
	static UE_API FLoadingScreenOverrideConfig GetLoadingScreenOverrideConfig(const UObject* WorldContextObject);
};

#undef UE_API
