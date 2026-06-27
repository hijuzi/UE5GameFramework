// Copyright Epic Games, Inc. All Rights Reserved.

#include "RefreshPSOCacheAssetListCommandlet.h"
#include "PSOCacheAssetList.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"

URefreshPSOCacheAssetListCommandlet::URefreshPSOCacheAssetListCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 URefreshPSOCacheAssetListCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("PSO Cache AssetList Refresh Commandlet"));
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	// 解析参数中的 PackagePath，默认路径
	FString PackagePath = TEXT("/PSOCacheSystem/DA/DA_PSOCacheAssetList");
	if (FParse::Value(*Params, TEXT("PackagePath="), PackagePath))
	{
		UE_LOG(LogTemp, Display, TEXT("Using custom package path: %s"), *PackagePath);
	}

	// 1. 等待 AssetRegistry 扫描完成
	UE_LOG(LogTemp, Display, TEXT("Waiting for AssetRegistry to finish scanning..."));
	{
		FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		AssetRegistry.SearchAllAssets(/*bSynchronousSearch=*/true);

		while (AssetRegistry.IsLoadingAssets())
		{
			FPlatformProcess::Sleep(0.1f);
		}

		TArray<FAssetData> AllAssets;
		AssetRegistry.GetAllAssets(AllAssets, /*bIncludeOnlyOnDiskAssets=*/false);
		UE_LOG(LogTemp, Display, TEXT("AssetRegistry scan complete. Total assets: %d"), AllAssets.Num());
	}

	// 2. 尝试加载已有的 DataAsset，若不存在则创建
	UPSOCacheAssetList* AssetList = LoadObject<UPSOCacheAssetList>(
		nullptr, *PackagePath, nullptr, LOAD_NoWarn);

	if (!AssetList)
	{
		UE_LOG(LogTemp, Display, TEXT("DataAsset not found, creating new one at: %s"), *PackagePath);

		UPackage* Package = CreatePackage(*PackagePath);
		AssetList = NewObject<UPSOCacheAssetList>(
			Package,
			UPSOCacheAssetList::StaticClass(),
			FName(*FPaths::GetBaseFilename(PackagePath)),
			RF_Public | RF_Standalone | RF_MarkAsRootSet);

		if (!AssetList)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create UPSOCacheAssetList!"));
			return 1;
		}

		FAssetRegistryModule::AssetCreated(AssetList);
		UE_LOG(LogTemp, Display, TEXT("New UPSOCacheAssetList created successfully."));
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("Loaded existing UPSOCacheAssetList from: %s"), *PackagePath);
	}

	// 3. 执行刷新
	UE_LOG(LogTemp, Display, TEXT("Refreshing asset list from AssetRegistry..."));
	AssetList->RefreshFromAssetRegistry();

	// 4. 保存 DataAsset
	UPackage* Package = AssetList->GetOutermost();
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;

	const bool bSaved = UPackage::SavePackage(Package, AssetList, *PackageFileName, SaveArgs);

	if (bSaved)
	{
		UE_LOG(LogTemp, Display, TEXT("========================================"));
		UE_LOG(LogTemp, Display, TEXT("Refresh Complete: %d Materials, %d NiagaraSystems"),
			AssetList->Materials.Num(), AssetList->NiagaraSystems.Num());
		UE_LOG(LogTemp, Display, TEXT("Saved to: %s"), *PackageFileName);
		UE_LOG(LogTemp, Display, TEXT("========================================"));
		return 0;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save UPSOCacheAssetList to: %s"), *PackageFileName);
		return 1;
	}
}
