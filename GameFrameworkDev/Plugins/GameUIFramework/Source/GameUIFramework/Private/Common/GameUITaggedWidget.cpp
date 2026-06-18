// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/GameUITaggedWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameUITaggedWidget)

//@TODO: 此文件中的其他 TODO 均与基于标签的控件显示/隐藏相关，参见 UE-142237

UGameUITaggedWidget::UGameUITaggedWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGameUITaggedWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsDesignTime())
	{
		// 监听隐藏标签的标签变化
		//@TODO: 之前提到的事情

		// 设置初始可见性值（检查标签等...）
		SetVisibility(GetVisibility());
	}
}

void UGameUITaggedWidget::NativeDestruct()
{
	if (!IsDesignTime())
	{
		//@TODO: 停止监听标签变化
	}

	Super::NativeDestruct();
}

void UGameUITaggedWidget::SetVisibility(ESlateVisibility InVisibility)
{
#if WITH_EDITORONLY_DATA
	if (IsDesignTime())
	{
		Super::SetVisibility(InVisibility);
		return;
	}
#endif

	// 记住调用者请求的值；即使当前被标签抑制，
	// 在抑制结束后我们也应遵循此请求
	bWantsToBeVisible = ConvertSerializedVisibilityToRuntime(InVisibility).IsVisible();
	if (bWantsToBeVisible)
	{
		ShownVisibility = InVisibility;
	}
	else
	{
		HiddenVisibility = InVisibility;
	}

	const bool bHasHiddenTags = false;//@TODO: Foo->HasAnyTags(HiddenByTags);

	// 实际应用可见性
	const ESlateVisibility DesiredVisibility = (bWantsToBeVisible && !bHasHiddenTags) ? ShownVisibility : HiddenVisibility;
	if (GetVisibility() != DesiredVisibility)
	{
		Super::SetVisibility(DesiredVisibility);
	}
}

void UGameUITaggedWidget::OnWatchedTagsChanged()
{
	const bool bHasHiddenTags = false;//@TODO: Foo->HasAnyTags(HiddenByTags);

	// 实际应用可见性
	const ESlateVisibility DesiredVisibility = (bWantsToBeVisible && !bHasHiddenTags) ? ShownVisibility : HiddenVisibility;
	if (GetVisibility() != DesiredVisibility)
	{
		Super::SetVisibility(DesiredVisibility);
	}
}
