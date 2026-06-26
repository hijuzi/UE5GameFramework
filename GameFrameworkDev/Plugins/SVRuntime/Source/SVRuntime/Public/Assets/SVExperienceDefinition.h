// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "CommonLoadingScreenSettings.h"

#include "SVExperienceDefinition.generated.h"

#define UE_API SVRUNTIME_API

class UCommonActivatableWidget;
class UWorld;

/**
 * USVBaseExperienceDefinition
 *
 * Experience 定义基类，配置一个 Gameplay Experience 所需的通用 UI 界面。
 * 参考 Lyra ULyraExperienceDefinition。
 */
UCLASS(BlueprintType, Const)
class USVBaseExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USVBaseExperienceDefinition();

	/** 从指定 World 的 WorldSettings 中获取当前 ExperienceDefinition（CDO） */
	UFUNCTION(BlueprintCallable, Category = "Experience", meta = (WorldContext = "WorldContextObject"))
	static UE_API const USVBaseExperienceDefinition* GetCurrentExperienceDefinition(const UObject* WorldContextObject);

	//~ Begin UObject Interface
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~ End UObject Interface

	//~ Begin UPrimaryDataAsset Interface
#if WITH_EDITORONLY_DATA
	UE_API virtual void UpdateAssetBundleData() override;
#endif
	//~ End UPrimaryDataAsset Interface

public:
	/** 主界面类 */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> MainScreenClass;

	/** 覆盖加载界面的时间参数（启用后下方 Duration 参数生效） */
	UPROPERTY(EditAnywhere, Category = "Loading Screen|Timing")
	bool bOverrideLoadingScreenTiming = false;

	/** 覆盖加载界面淡入动画时长（秒） */
	UPROPERTY(EditAnywhere, Category = "Loading Screen|Timing", meta = (ForceUnits = s, ClampMin = "0.0", ClampMax = "5.0", EditCondition = "bOverrideLoadingScreenTiming"))
	float LoadingScreenLoadDurationOverride = 0.5f;

	/** 覆盖加载界面淡出动画时长（秒） */
	UPROPERTY(EditAnywhere, Category = "Loading Screen|Timing", meta = (ForceUnits = s, ClampMin = "0.0", ClampMax = "5.0", EditCondition = "bOverrideLoadingScreenTiming"))
	float LoadingScreenUnloadDurationOverride = 0.5f;

	/** 覆盖加载界面动画过渡类型 */
	UPROPERTY(EditAnywhere, Category = "Loading Screen|Timing", meta = (EditCondition = "bOverrideLoadingScreenTiming"))
	ELoadingAnimationType LoadingScreenAnimationTypeOverride = ELoadingAnimationType::Opacity;

	/** 覆盖加载界面动画插值模式 */
	UPROPERTY(EditAnywhere, Category = "Loading Screen|Timing", meta = (EditCondition = "bOverrideLoadingScreenTiming"))
	ELoadingAnimationMode LoadingScreenAnimationModeOverride = ELoadingAnimationMode::Linear;

	/** 覆盖加载界面的内容参数（启用后下方 Content 参数生效） */
	UPROPERTY(EditAnywhere, Category = "Loading Screen|Content")
	bool bOverrideLoadingScreenContent = false;

	/** 覆盖加载界面内容类型 */
	UPROPERTY(EditAnywhere, Category = "Loading Screen|Content", meta = (EditCondition = "bOverrideLoadingScreenContent"))
	ELoadingScreenContentType LoadingScreenContentTypeOverride = ELoadingScreenContentType::Image;

	/** 覆盖图片背景资产（ContentType 为 Image 时生效） */
	UPROPERTY(EditAnywhere, Category = "Loading Screen|Content", meta = (MetaClass = "/Script/Engine.Texture2D", EditCondition = "bOverrideLoadingScreenContent && LoadingScreenContentTypeOverride == ELoadingScreenContentType::Image"))
	FSoftObjectPath LoadingScreenImageBackgroundOverride;

	/** 覆盖视频路径（ContentType 为 Video 时生效） */
	UPROPERTY(EditAnywhere, Category = "Loading Screen|Content", meta = (EditCondition = "bOverrideLoadingScreenContent && LoadingScreenContentTypeOverride == ELoadingScreenContentType::Video"))
	FString LoadingScreenVideoPathOverride;

	/** 覆盖黑屏的时间参数（启用后下方 Duration 参数生效） */
	UPROPERTY(EditAnywhere, Category = "Black Screen|Timing")
	bool bOverrideBlackScreenTiming = false;

	/** 覆盖黑屏淡入动画时长（秒） */
	UPROPERTY(EditAnywhere, Category = "Black Screen|Timing", meta = (ForceUnits = s, ClampMin = "0.0", ClampMax = "5.0", EditCondition = "bOverrideBlackScreenTiming"))
	float BlackScreenLoadDurationOverride = 0.5f;

	/** 覆盖黑屏淡出动画时长（秒） */
	UPROPERTY(EditAnywhere, Category = "Black Screen|Timing", meta = (ForceUnits = s, ClampMin = "0.0", ClampMax = "5.0", EditCondition = "bOverrideBlackScreenTiming"))
	float BlackScreenUnloadDurationOverride = 0.5f;

	/** 覆盖黑屏动画过渡类型 */
	UPROPERTY(EditAnywhere, Category = "Black Screen|Timing", meta = (EditCondition = "bOverrideBlackScreenTiming"))
	ELoadingAnimationType BlackScreenAnimationTypeOverride = ELoadingAnimationType::Opacity;

	/** 覆盖黑屏动画插值模式 */
	UPROPERTY(EditAnywhere, Category = "Black Screen|Timing", meta = (EditCondition = "bOverrideBlackScreenTiming"))
	ELoadingAnimationMode BlackScreenAnimationModeOverride = ELoadingAnimationMode::Linear;
};

/**
 * USVLoginExperienceDefinition
 *
 * Login Experience 定义，在 Base 基础上增加 Press Start 和着色器编译界面配置。
 */
UCLASS(BlueprintType, Const)
class USVLoginExperienceDefinition : public USVBaseExperienceDefinition
{
	GENERATED_BODY()

public:
	/** 从指定 World 的 WorldSettings 中获取当前 Login ExperienceDefinition（CDO），内部自动 Cast */
	UFUNCTION(BlueprintCallable, Category = "Experience", meta = (WorldContext = "WorldContextObject"))
	static UE_API const USVLoginExperienceDefinition* GetCurrentLoginExperienceDefinition(const UObject* WorldContextObject);

	/** Press Start 界面类 */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> PressStartScreenClass;

	/** 着色器编译界面类 */
	UPROPERTY(EditAnywhere, Category = "UI|Shader Screen")
	TSoftClassPtr<UCommonActivatableWidget> CompilingShadersScreenClass;

	/** 是否强制开启着色器编译界面 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Shader Screen")
	bool bForceShowCompilingShadersScreen = false;

	/** 强制开启着色器界面的持续时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Shader Screen", meta = (ForceUnits = s, ClampMin = "2.0", ClampMax = "10.0", EditCondition = "bForceShowCompilingShadersScreen"))
	float CompilingShadersScreenForceDuration = 3.0f;

	/** 编辑器中是否也需要开启着色器界面 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Shader Screen", meta = (EditCondition = "bForceShowCompilingShadersScreen"))
	bool bEnableCompilingShadersInEditor = false;
};

#undef UE_API
