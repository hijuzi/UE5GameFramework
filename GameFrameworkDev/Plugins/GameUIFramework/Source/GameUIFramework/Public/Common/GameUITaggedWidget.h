// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"

#include "GameUITaggedWidget.generated.h"

class UObject;

/**
 * 布局中已打标签的控件（可以通过所属玩家的标签来隐藏或显示）
 */
UCLASS(Abstract, Blueprintable)
class UGameUITaggedWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UGameUITaggedWidget(const FObjectInitializer& ObjectInitializer);

	//~UWidget 接口
	virtual void SetVisibility(ESlateVisibility InVisibility) override;
	//~End of UWidget 接口

	//~UUserWidget 接口
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~End of UUserWidget 接口

protected:
	/** 如果所属玩家拥有这些标签中的任意一个，此控件将被隐藏（使用 HiddenVisibility） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FGameplayTagContainer HiddenByTags;

	/** 此控件显示时（未被 Gameplay 标签隐藏时）使用的可见性。 */
	UPROPERTY(EditAnywhere, Category = "HUD")
	ESlateVisibility ShownVisibility = ESlateVisibility::Visible;

	/** 此控件被 Gameplay 标签隐藏时使用的可见性。 */
	UPROPERTY(EditAnywhere, Category = "HUD")
	ESlateVisibility HiddenVisibility = ESlateVisibility::Collapsed;

	/** 我们是否想要可见（忽略标签）？ */
	bool bWantsToBeVisible = true;

private:
	void OnWatchedTagsChanged();
};
