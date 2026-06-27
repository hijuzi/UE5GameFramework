// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_PSOCacheAssetList.h"
#include "PSOCacheAssetList.h"

#include "ToolMenus.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "PSOCacheAssetListActions"

FText FAssetTypeActions_PSOCacheAssetList::GetName() const
{
	return LOCTEXT("AssetTypeActions_PSOCacheAssetList", "PSO Cache Asset List");
}

FColor FAssetTypeActions_PSOCacheAssetList::GetTypeColor() const
{
	return FColor(64, 192, 255);
}

UClass* FAssetTypeActions_PSOCacheAssetList::GetSupportedClass() const
{
	return UPSOCacheAssetList::StaticClass();
}

uint32 FAssetTypeActions_PSOCacheAssetList::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

void FAssetTypeActions_PSOCacheAssetList::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	FAssetTypeActions_Base::GetActions(InObjects, Section);

	Section.AddMenuEntry(
		"RefreshPSOCacheAssetList",
		LOCTEXT("RefreshPSOCacheAssetList", "Refresh From Asset Registry"),
		LOCTEXT("RefreshPSOCacheAssetListTooltip", "Scan all Materials and NiagaraSystems in the project and update this asset list."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
		FUIAction(
			FExecuteAction::CreateLambda([InObjects]()
			{
				for (UObject* Obj : InObjects)
				{
					if (UPSOCacheAssetList* AssetList = Cast<UPSOCacheAssetList>(Obj))
					{
						AssetList->RefreshFromAssetRegistry();

						UPackage* Package = AssetList->GetOutermost();
						const FString PackageFileName = FPackageName::LongPackageNameToFilename(
							Package->GetName(), FPackageName::GetAssetPackageExtension());

						FSavePackageArgs SaveArgs;
						SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
						SaveArgs.SaveFlags = SAVE_NoError;

						UPackage::SavePackage(Package, AssetList, *PackageFileName, SaveArgs);
					}
				}
			})
		)
	);
}

#undef LOCTEXT_NAMESPACE
