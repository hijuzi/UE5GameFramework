// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSOCacheAssetList.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "NiagaraSystem.h"

void UPSOCacheAssetList::RefreshFromAssetRegistry()
{
	const FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// 确保 AssetRegistry 扫描完成
	if (AssetRegistry.IsLoadingAssets())
	{
		AssetRegistry.SearchAllAssets(/*bSynchronousSearch=*/true);
	}

	// 1. 扫描所有 Material 资源（含子类）
	Materials.Reset();
	{
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByClass(
			UMaterial::StaticClass()->GetClassPathName(),
			AssetDataList,
			/*bSearchSubClasses=*/true);

		for (const FAssetData& AssetData : AssetDataList)
		{
			Materials.Add(TSoftObjectPtr<UMaterial>(AssetData.GetSoftObjectPath()));
		}

		UE_LOG(LogTemp, Log, TEXT("UPSOCacheAssetList::RefreshFromAssetRegistry - Found %d Materials"), Materials.Num());
	}

	// 2. 扫描所有 NiagaraSystem 资源
	NiagaraSystems.Reset();
	{
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByClass(
			UNiagaraSystem::StaticClass()->GetClassPathName(),
			AssetDataList,
			/*bSearchSubClasses=*/true);

		for (const FAssetData& AssetData : AssetDataList)
		{
			NiagaraSystems.Add(TSoftObjectPtr<UNiagaraSystem>(AssetData.GetSoftObjectPath()));
		}

		UE_LOG(LogTemp, Log, TEXT("UPSOCacheAssetList::RefreshFromAssetRegistry - Found %d NiagaraSystems"), NiagaraSystems.Num());
	}

	// 标记包为脏，以便保存
	MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("UPSOCacheAssetList::RefreshFromAssetRegistry - Complete: %d Materials, %d NiagaraSystems"),
		Materials.Num(), NiagaraSystems.Num());
}
