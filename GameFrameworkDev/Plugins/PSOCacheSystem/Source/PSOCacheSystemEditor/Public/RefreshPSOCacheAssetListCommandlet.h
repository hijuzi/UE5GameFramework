// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "RefreshPSOCacheAssetListCommandlet.generated.h"

/**
 * 刷新 PSO 缓存资产列表的 Commandlet
 * 
 * 用法：
 *   UE5Editor-Cmd.exe Project.uproject -run=RefreshPSOCacheAssetList
 * 
 * 可选参数：
 *   -PackagePath=/Game/PSOCache/DA_PSOCacheAssetList  指定 DataAsset 路径
 */
UCLASS()
class PSOCACHESYSTEMEDITOR_API URefreshPSOCacheAssetListCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URefreshPSOCacheAssetListCommandlet();

	virtual int32 Main(const FString& Params) override;
};
