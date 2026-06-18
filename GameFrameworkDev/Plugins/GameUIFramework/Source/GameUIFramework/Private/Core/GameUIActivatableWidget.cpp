// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/GameUIActivatableWidget.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameUIActivatableWidget)

#define LOCTEXT_NAMESPACE "GameUIFramework"

UGameUIActivatableWidget::UGameUIActivatableWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TOptional<FUIInputConfig> UGameUIActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputConfig)
	{
	case EGameUIWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case EGameUIWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case EGameUIWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case EGameUIWidgetInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}

#if WITH_EDITOR

void UGameUIActivatableWidget::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);

	if (!GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UGameUIActivatableWidget, BP_GetDesiredFocusTarget)))
	{
		if (GetParentNativeClass(GetClass()) == UGameUIActivatableWidget::StaticClass())
		{
			CompileLog.Warning(LOCTEXT("ValidateGetDesiredFocusTarget_Warning", "GetDesiredFocusTarget 未实现，在此屏幕上使用手柄时可能会遇到麻烦。"));
		}
		else
		{
			//TODO - 注意：目前无法保证它不在原生子类中实现。
			CompileLog.Note(LOCTEXT("ValidateGetDesiredFocusTarget_Note", "GetDesiredFocusTarget 未实现，在此屏幕上使用手柄时可能会遇到麻烦。如果已在原生基类中实现，可以忽略此消息。"));
		}
	}
}

#endif

#undef LOCTEXT_NAMESPACE
