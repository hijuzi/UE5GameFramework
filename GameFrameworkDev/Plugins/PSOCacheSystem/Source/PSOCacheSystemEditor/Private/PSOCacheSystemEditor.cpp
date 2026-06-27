// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSOCacheSystemEditor.h"
#include "AssetTypeActions_PSOCacheAssetList.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "FPSOCacheSystemEditorModule"

void FPSOCacheSystemEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	PSOCacheAssetListActions = MakeShareable(new FAssetTypeActions_PSOCacheAssetList);
	AssetTools.RegisterAssetTypeActions(PSOCacheAssetListActions.ToSharedRef());
}

void FPSOCacheSystemEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(PSOCacheAssetListActions.ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPSOCacheSystemEditorModule, PSOCacheSystemEditor)
