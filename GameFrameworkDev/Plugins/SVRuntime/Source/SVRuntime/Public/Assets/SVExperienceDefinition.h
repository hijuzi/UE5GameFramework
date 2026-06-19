// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "SVExperienceDefinition.generated.h"

#define UE_API SVRUNTIME_API

class UCommonActivatableWidget;

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
	/** 该 Experience 是否使用 Loading Screen（关闭后流程中不显示任何 Loading 界面） */
	UPROPERTY(EditAnywhere, Category = "UI")
	bool bUseLoadingScreen = true;

	/** 主界面类 */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> MainScreenClass;
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
	/** Press Start 界面类 */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> PressStartScreenClass;

	/** 着色器编译界面类 */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> CompilingShadersScreenClass;
};

#undef UE_API
