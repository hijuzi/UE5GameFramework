// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonHardwareVisibilityBorder.h"

#include "CWHardwareVisibilityBorder.generated.h"

/**
 * 硬件可见性边界，根据当前输入设备类型（手柄/键鼠）控制子控件的显隐。
 * 相比基类提供了蓝图可重写的输入设备变更回调，便于在蓝图子类中扩展逻辑。
 */
UCLASS(BlueprintType, Blueprintable)
class COMMONUSERWIDGETBASE_API UCWHardwareVisibilityBorder : public UCommonHardwareVisibilityBorder
{
	GENERATED_BODY()

protected:
	//~ Begin UWidget interface
	virtual void OnWidgetRebuilt() override;
	//~ End UWidget interface

	/** 输入设备变更时触发，供蓝图子类重写（例如切换按钮样式、提示文本等）*/
	UFUNCTION(BlueprintImplementableEvent, Category = "Hardware Visibility")
	void OnHardwareInputMethodChanged();

private:
	/** 绑定 UCommonUIVisibilitySubsystem 的可见性变更委托 */
	void BindVisibilityChangedDelegate();

	/** 委托句柄，用于在销毁时解除绑定 */
	FDelegateHandle VisibilityChangedHandle;

public:
	/** 当前 Widget 的 Visibility 是否等于 VisibleType */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hardware Visibility")
	bool IsInVisibleState() const;

	/** 当前 Widget 的 Visibility 是否等于 HiddenType */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hardware Visibility")
	bool IsInHiddenState() const;
};
