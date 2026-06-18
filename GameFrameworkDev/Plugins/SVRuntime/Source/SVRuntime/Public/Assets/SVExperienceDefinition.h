// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "SVExperienceDefinition.generated.h"

#define UE_API SVRUNTIME_API

class UCommonActivatableWidget;

/**
 * USVExperienceDefinition
 *
 * Experience 定义资产，配置一个 Gameplay Experience 所需的 UI 界面。
 * 参考 Lyra ULyraExperienceDefinition。
 */
UCLASS(BlueprintType, Const)
class USVExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USVExperienceDefinition();

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
	/** Press Start 界面类 */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> PressStartScreenClass;

	/** 主界面类 */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> MainScreenClass;
};

#undef UE_API
