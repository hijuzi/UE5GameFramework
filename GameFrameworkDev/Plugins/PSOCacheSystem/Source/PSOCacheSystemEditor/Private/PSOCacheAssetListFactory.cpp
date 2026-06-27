// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSOCacheAssetListFactory.h"
#include "PSOCacheAssetList.h"

UPSOCacheAssetListFactory::UPSOCacheAssetListFactory()
{
	SupportedClass = UPSOCacheAssetList::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UPSOCacheAssetListFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UPSOCacheAssetList>(InParent, InClass, InName, Flags);
}
