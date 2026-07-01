// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/CWHardwareVisibilityBorder.h"
#include "CommonUIVisibilitySubsystem.h"
#include "Engine/LocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CWHardwareVisibilityBorder)

void UCWHardwareVisibilityBorder::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();
	BindVisibilityChangedDelegate();
}

void UCWHardwareVisibilityBorder::BindVisibilityChangedDelegate()
{
	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UCommonUIVisibilitySubsystem* VisSystem = UCommonUIVisibilitySubsystem::Get(LocalPlayer))
		{
			VisibilityChangedHandle = VisSystem->OnVisibilityTagsChanged.AddWeakLambda(this,
				[this](UCommonUIVisibilitySubsystem*)
				{
					OnHardwareInputMethodChanged();
				});
		}
	}
}

bool UCWHardwareVisibilityBorder::IsInVisibleState() const
{
	return GetVisibility() == VisibleType;
}

bool UCWHardwareVisibilityBorder::IsInHiddenState() const
{
	return GetVisibility() == HiddenType;
}
