// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

/**
 * 为 UPSOCacheAssetList 添加 Content Browser 右键菜单中的 "Refresh From Asset Registry" 操作
 */
class FAssetTypeActions_PSOCacheAssetList : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	/** 在右键菜单中添加自定义操作 */
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
};
