// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "PSOCacheAssetListFactory.generated.h"

/**
 * Factory for creating UPSOCacheAssetList DataAsset via Content Browser right-click menu
 */
UCLASS()
class PSOCACHESYSTEMEDITOR_API UPSOCacheAssetListFactory : public UFactory
{
	GENERATED_BODY()

public:
	UPSOCacheAssetListFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
