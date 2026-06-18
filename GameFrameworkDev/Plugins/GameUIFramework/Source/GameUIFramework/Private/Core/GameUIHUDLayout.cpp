// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/GameUIHUDLayout.h"

#include "Extensions/GameUIExtensions.h"
#include "GameplayTags/CommonGameplayTags.h"
#include "CommonUISettings.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/InputSettings.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Input/CommonUIInputTypes.h"
#include "ICommonUIModule.h"
#include "LogGameUIFramework.h"
#include "NativeGameplayTags.h"

#if WITH_EDITOR
#include "CommonUIVisibilitySubsystem.h"
#endif	// WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameUIHUDLayout)

// 所有 GameplayTag 定义统一由 CommonGameplayTags 管理（位于 CommonUtility 插件）。
// 使用 CommonGameplayTags::TAG_XXX 即可引用。
using namespace CommonGameplayTags;

// ============================================================================
//  构造 & 生命周期
// ============================================================================

UGameUIHUDLayout::UGameUIHUDLayout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SpawnedControllerDisconnectScreen(nullptr)
{
	// 默认策略：仅主手柄平台需要手柄断开屏幕
	// 蓝图可叠加或移除此 Tag
	PlatformRequiresControllerDisconnectScreen.AddTag(TAG_PLATFORM_TRAIT_INPUT_PRIMARILYCONTROLLER);
}

void UGameUIHUDLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 1) 注册 Escape 动作绑定
	RegisterUIActionBinding(FBindUIActionArgs(
		FUIActionTag::ConvertChecked(TAG_GAMEUI_ACTION_ESCAPE),
		false,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleEscapeAction)));

	// 2) 仅在需要手柄断开屏幕的平台上绑定设备事件
	if (ShouldPlatformDisplayControllerDisconnectScreen())
	{
		IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
		DeviceMapper.GetOnInputDeviceConnectionChange().AddUObject(this, &ThisClass::HandleInputDeviceConnectionChanged);
		DeviceMapper.GetOnInputDevicePairingChange().AddUObject(this, &ThisClass::HandleInputDevicePairingChanged);
	}
}

void UGameUIHUDLayout::NativeDestruct()
{
	// 对称清理：解绑所有委托 + 取消未执行的 Ticker
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	DeviceMapper.GetOnInputDeviceConnectionChange().RemoveAll(this);
	DeviceMapper.GetOnInputDevicePairingChange().RemoveAll(this);

	if (RequestProcessControllerStateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RequestProcessControllerStateHandle);
		RequestProcessControllerStateHandle.Reset();
	}

	Super::NativeDestruct();
}

// ============================================================================
//  Escape 菜单
// ============================================================================

void UGameUIHUDLayout::HandleEscapeAction()
{
	if (ensure(!EscapeMenuClass.IsNull()))
	{
		// 异步加载 + 实例化 + Push 到 FullScreenMenu Layer（GameUI.Layer.FullScreenMenu）
		// PushStreamedContentToLayer_ForPlayer 封装了这三步操作
		UGameUIExtensions::PushStreamedContentToLayer_ForPlayer(
			GetOwningLocalPlayer(), TAG_GAMEUI_LAYER_FULLSCREENMENU, EscapeMenuClass);
	}
}

// ============================================================================
//  手柄断开检测
// ============================================================================

void UGameUIHUDLayout::HandleInputDeviceConnectionChanged(
	EInputDeviceConnectionState NewConnectionState,
	FPlatformUserId PlatformUserId,
	FInputDeviceId InputDeviceId)
{
	const FPlatformUserId OwningLocalPlayerId = GetOwningLocalPlayer()->GetPlatformUserId();
	ensure(OwningLocalPlayerId.IsValid());

	// 仅处理属于当前玩家的设备事件，其他玩家的事件直接忽略
	if (PlatformUserId != OwningLocalPlayerId)
	{
		return;
	}

	NotifyControllerStateChangeForDisconnectScreen();
}

void UGameUIHUDLayout::HandleInputDevicePairingChanged(
	FInputDeviceId InputDeviceId,
	FPlatformUserId NewUserPlatformId,
	FPlatformUserId OldUserPlatformId)
{
	const FPlatformUserId OwningLocalPlayerId = GetOwningLocalPlayer()->GetPlatformUserId();
	ensure(OwningLocalPlayerId.IsValid());

	// 若设备移入/移出当前玩家，都需要重新评估是否需要断开屏幕
	if (NewUserPlatformId == OwningLocalPlayerId || OldUserPlatformId == OwningLocalPlayerId)
	{
		NotifyControllerStateChangeForDisconnectScreen();
	}
}

bool UGameUIHUDLayout::ShouldPlatformDisplayControllerDisconnectScreen() const
{
	// 1) 运行时判断：读取 INI 配置的 PlatformTraits
	bool bHasAllRequiredTags = ICommonUIModule::GetSettings()
		.GetPlatformTraits()
		.HasAll(PlatformRequiresControllerDisconnectScreen);

	// 2) 编辑器模拟：叠加 UCommonUIVisibilitySubsystem 的模拟 Tag
	//    使用 |= 确保编辑器模拟 Tag 叠加到运行时 Tag 之上
#if WITH_EDITOR
	const FGameplayTagContainer& PlatformEmulationTags =
		UCommonUIVisibilitySubsystem::Get(GetOwningLocalPlayer())->GetVisibilityTags();
	bHasAllRequiredTags |= PlatformEmulationTags.HasAll(PlatformRequiresControllerDisconnectScreen);
#endif	// WITH_EDITOR

	return bHasAllRequiredTags;
}

void UGameUIHUDLayout::NotifyControllerStateChangeForDisconnectScreen()
{
	ensure(ShouldPlatformDisplayControllerDisconnectScreen());

	// 防止重复排队：仅在尚未排队时将处理逻辑延迟到下一帧
	// CreateWeakLambda 确保 Widget 销毁后回调不会 crash
	if (!RequestProcessControllerStateHandle.IsValid())
	{
		RequestProcessControllerStateHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
			{
				RequestProcessControllerStateHandle.Reset();
				ProcessControllerDevicesHavingChangedForDisconnectScreen();
				return false; // 仅执行一次，自动注销
			}));
	}
}

void UGameUIHUDLayout::ProcessControllerDevicesHavingChangedForDisconnectScreen()
{
	ensure(ShouldPlatformDisplayControllerDisconnectScreen());

	const FPlatformUserId OwningLocalPlayerId = GetOwningLocalPlayer()->GetPlatformUserId();
	ensure(OwningLocalPlayerId.IsValid());

	// 获取映射到当前玩家的所有输入设备
	const IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	TArray<FInputDeviceId> MappedInputDevices;
	const int32 NumDevicesMappedToUser = DeviceMapper.GetAllInputDevicesForUser(
		OwningLocalPlayerId, OUT MappedInputDevices);

	// 遍历所有映射设备，检查是否存在已连接的 Gamepad
	bool bHasConnectedController = false;
	for (const FInputDeviceId MappedDevice : MappedInputDevices)
	{
		if (DeviceMapper.GetInputDeviceConnectionState(MappedDevice) == EInputDeviceConnectionState::Connected)
		{
			const FHardwareDeviceIdentifier HardwareInfo =
				UInputDeviceSubsystem::Get()->GetInputDeviceHardwareIdentifier(MappedDevice);
			if (HardwareInfo.PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad)
			{
				bHasConnectedController = true;
				break; // 找到一个就够了
			}
		}
	}

	// 决策：
	//  - 无 Gamepad → 弹出断开提示
	//  - 有 Gamepad + 断开屏正在显示 → 隐藏断开屏
	if (!bHasConnectedController)
	{
		DisplayControllerDisconnectedMenu();
	}
	else if (SpawnedControllerDisconnectScreen)
	{
		HideControllerDisconnectedMenu();
	}
}

// ============================================================================
//  BlueprintNativeEvent 实现
// ============================================================================

void UGameUIHUDLayout::DisplayControllerDisconnectedMenu_Implementation()
{
	UE_LOG(LogGameUIFramework, Log, TEXT("[%hs] 显示控制器断开菜单！"), __func__);

	if (ControllerDisconnectedScreen)
	{
		// PushContentToLayer_ForPlayer：同步创建 Widget 并推入指定 Layer
		SpawnedControllerDisconnectScreen = UGameUIExtensions::PushContentToLayer_ForPlayer(
			GetOwningLocalPlayer(), TAG_GAMEUI_LAYER_FULLSCREENMENU, ControllerDisconnectedScreen);
	}
}

void UGameUIHUDLayout::HideControllerDisconnectedMenu_Implementation()
{
	UE_LOG(LogGameUIFramework, Log, TEXT("[%hs] 隐藏控制器断开菜单！"), __func__);

	// 从 Layer 弹出断开屏，并清空追踪指针
	UGameUIExtensions::PopContentFromLayer(SpawnedControllerDisconnectScreen);
	SpawnedControllerDisconnectScreen = nullptr;
}
