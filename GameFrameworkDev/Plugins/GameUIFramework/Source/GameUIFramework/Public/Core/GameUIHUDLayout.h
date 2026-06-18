// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/GameUIActivatableWidget.h"
#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"

#include "GameUIHUDLayout.generated.h"

class UCommonActivatableWidget;
class UObject;

/**
 * UGameUIHUDLayout
 *
 * 从 Lyra 的 ULyraHUDLayout 借鉴而来，是玩家视角下所有 HUD 元素的根容器。
 * 由 Experience 通过 Add Widgets Action 动态注入到 UPrimaryGameUILayout 的指定 Layer
 * （通常为 UI.Layer.Game），不硬编码使用。
 *
 * 继承链：UCommonActivatableWidget → UGameUIActivatableWidget → UGameUIHUDLayout
 *
 * 核心职责：
 *  1. Escape 菜单（暂停/设置面板）—— 软引用 + 按需异步加载
 *  2. 手柄断开检测 —— 平台 Tag 驱动 + FTSTicker 延迟处理
 */
UCLASS(Abstract, BlueprintType, Blueprintable, Meta = (DisplayName = "GameUI HUD Layout", Category = "GameUIFramework|HUD"))
class UGameUIHUDLayout : public UGameUIActivatableWidget
{
	GENERATED_BODY()

public:

	UGameUIHUDLayout(const FObjectInitializer& ObjectInitializer);

	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

protected:
	// ========================================================================
	//  Escape 菜单
	// ========================================================================

	/**
	 * Escape / 暂停键的回调。
	 * 通过 CommonUI 输入路由系统触发，异步加载 EscapeMenuClass 并推送至 UI.Layer.Menu。
	 */
	void HandleEscapeAction();

	// ========================================================================
	//  手柄断开检测
	// ========================================================================

	/**
	 * 输入设备连接状态变化时的回调。
	 * 仅处理属于当前玩家的设备事件；若玩家失去所有 Gamepad，则弹出断开提示。
	 */
	void HandleInputDeviceConnectionChanged(EInputDeviceConnectionState NewConnectionState, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId);

	/**
	 * 输入设备配对关系变化时的回调（设备更换所属 PlatformUserId）。
	 * 若新旧任一 User 匹配当前玩家，则重新评估是否需要显示断开屏幕。
	 */
	void HandleInputDevicePairingChanged(FInputDeviceId InputDeviceId, FPlatformUserId NewUserPlatformId, FPlatformUserId OldUserPlatformId);

	/**
	 * 通知状态变化，排队到下一帧处理。
	 *
	 * 为什么延迟一帧？
	 * 设备变化回调可能发生在 Widget Tick / 输入处理 / Layer Push 的中间态，
	 * 此时直接 Push/Pop 会破坏 CommonUI Layer 栈状态。延迟一帧确保安全。
	 */
	void NotifyControllerStateChangeForDisconnectScreen();

	/**
	 * 实际检查玩家连接的控制器状态：
	 *  - 无 Gamepad → DisplayControllerDisconnectedMenu()
	 *  - 有 Gamepad + 断开屏正在显示 → HideControllerDisconnectedMenu()
	 */
	virtual void ProcessControllerDevicesHavingChangedForDisconnectScreen();

	/**
	 * 判断当前平台是否需要"手柄断开"屏幕。
	 *
	 * 双源判断：
	 *  1. 运行时：读取 INI 配置的 PlatformTraits
	 *  2. 编辑器：叠加 UCommonUIVisibilitySubsystem 模拟的平台 Tag
	 */
	virtual bool ShouldPlatformDisplayControllerDisconnectScreen() const;

	/**
	 * 将 ControllerDisconnectedScreen 推入 Menu 层（UI.Layer.Menu）。
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Controller Disconnect Menu")
	void DisplayControllerDisconnectedMenu();

	/**
	 * 若控制器断开菜单正在显示，则隐藏并从 Layer 弹出。
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Controller Disconnect Menu")
	void HideControllerDisconnectedMenu();

	// ========================================================================
	//  属性
	// ========================================================================

	/**
	 * 按下暂停 / Escape 键时显示的菜单类。
	 *
	 * 使用 TSoftClassPtr（软引用）而非硬引用：
	 * Escape 菜单通常是一个大型 UI（含设置、退出游戏等子面板），
	 * 软引用确保仅在真正按键时才触发异步加载，不拖慢 HUD 初始化。
	 */
	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<UCommonActivatableWidget> EscapeMenuClass;

	/**
	 * 当用户的所有控制器都断开连接时应显示的控件类。
	 * 硬引用 —— 手柄断开是小概率但紧急事件，需要 UI 随时可用。
	 */
	UPROPERTY(EditDefaultsOnly, Category="Controller Disconnect Menu")
	TSubclassOf<UCommonActivatableWidget> ControllerDisconnectedScreen;

	/**
	 * 显示"手柄断开"屏幕所需的平台 Tag 条件。
	 *
	 * 默认包含 Platform.Trait.Input.PrimarlyController，表示仅主手柄平台需要。
	 * 若平台 INI 未配置这些 Tag，断开屏幕永远不会弹出。
	 * 蓝图可叠加更多条件 Tag（如 Platform.Trait.Input.HasStrictControllerPairing），
	 * 实现全平台适配。
	 */
	UPROPERTY(EditDefaultsOnly, Category="Controller Disconnect Menu")
	FGameplayTagContainer PlatformRequiresControllerDisconnectScreen;

	/** 指向当前活跃的"控制器已断开"菜单实例（如果有）。Transient 不参与序列化。 */
	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidget> SpawnedControllerDisconnectScreen;

	/**
	 * FTSTicker 句柄，用于将设备状态处理延迟到下一帧。
	 * 仅在已排队但尚未执行时有效，执行后自动 Reset。
	 */
	FTSTicker::FDelegateHandle RequestProcessControllerStateHandle;
};
