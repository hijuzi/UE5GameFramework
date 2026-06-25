// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSOCacheManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/GameInstance.h"
#include "Materials/Material.h"
#include "NiagaraSystem.h"
#include "RHI.h"

void UPSOCacheManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::Initialize - PSO Cache Manager Initialized"));
}

void UPSOCacheManager::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::Deinitialize - PSO Cache Manager Shutdown"));

	Super::Deinitialize();
}

bool UPSOCacheManager::IsPSOCacheReady() const
{
	return bPSOCacheReady;
}

int32 UPSOCacheManager::GetCachedPSOCount() const
{
	return CachedPSOCount;
}

void UPSOCacheManager::LoadBuiltInPSOCache()
{
	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::LoadBuiltInPSOCache - Loading built-in PSO cache..."));

	// TODO: 实现内建 PSO 缓存加载逻辑

	bPSOCacheReady = true;
}

TArray<TSoftObjectPtr<UMaterial>> UPSOCacheManager::GetAllMaterials()
{
	TArray<TSoftObjectPtr<UMaterial>> Materials;

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(UMaterial::StaticClass()->GetClassPathName(), AssetDataList, /*bSearchSubClasses=*/true);

	for (const FAssetData& AssetData : AssetDataList)
	{
		Materials.Add(TSoftObjectPtr<UMaterial>(AssetData.GetSoftObjectPath()));
	}

	return Materials;
}

TArray<TSoftObjectPtr<UNiagaraSystem>> UPSOCacheManager::GetAllNiagaraParticleSystems()
{
	TArray<TSoftObjectPtr<UNiagaraSystem>> NiagaraSystems;

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(UNiagaraSystem::StaticClass()->GetClassPathName(), AssetDataList, /*bSearchSubClasses=*/true);

	for (const FAssetData& AssetData : AssetDataList)
	{
		NiagaraSystems.Add(TSoftObjectPtr<UNiagaraSystem>(AssetData.GetSoftObjectPath()));
	}

	return NiagaraSystems;
}

void UPSOCacheManager::ClearPSOCache()
{
	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::ClearPSOCache - Clearing PSO cache..."));

	CachedPSOCount = 0;
	bPSOCacheReady = false;
}
