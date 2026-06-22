// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "UObject/SoftObjectPath.h"

#include "BlackScreenUserWidget.h"
#include "CommonLoadingScreenSettings.generated.h"

class UObject;

/**
 * 加载界面系统的设置。
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Common Loading Screen"))
class UCommonLoadingScreenSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	UCommonLoadingScreenSettings();

public:
	//~=========================================================================
	// 加载界面
	//~=========================================================================

	// 加载界面所使用的控件。
	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(MetaClass="/Script/UMG.UserWidget"))
	FSoftClassPath LoadingScreenWidget;

	// 加载界面控件在视口堆栈中的 Z 顺序
	UPROPERTY(config, EditAnywhere, Category=Loading)
	int32 LoadingScreenZOrder = 10000;

		// 加载界面遮罩淡入动画时长（秒）
	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(ForceUnits=s))
	float MaskFadeInDuration = 0.2f;

	// 加载界面遮罩淡出动画时长（秒）
	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(ForceUnits=s))
	float MaskFadeOutDuration = 0.2f;




	// 其他加载完成后额外保持加载界面的时长（秒），
	// 以便给纹理流式加载留出时间，避免画面模糊
	//
	// 注意：在编辑器中通常不应用此延迟，以加快迭代速度，但可以通过
	// HoldLoadingScreenAdditionalSecsEvenInEditor 启用
 	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(ForceUnits=s, ConsoleVariable="CommonLoadingScreen.HoldLoadingScreenAdditionalSecs"))
	float HoldLoadingScreenAdditionalSecs = 2.0f;

	// 即使在编辑器中也应用额外的 HoldLoadingScreenAdditionalSecs 延迟
	// （在迭代加载界面时有用）
	UPROPERTY(config, EditAnywhere, Category=Loading)
	bool HoldLoadingScreenAdditionalSecsEvenInEditor = false;

	// 超过此秒数（非零时）后加载界面被视为永久挂起。
 	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(ForceUnits=s))
	float LoadingScreenHeartbeatHangDuration = 0.0f;

	// 每隔多少秒输出一次保持加载界面的日志（非零时）。
 	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(ForceUnits=s))
	float LogLoadingScreenHeartbeatInterval = 5.0f;

	// 加载期间心跳挂起的倍率，放大心跳超时阈值以避免误判卡死。默认 1.0 表示不放大。
 	UPROPERTY(config, EditAnywhere, Category=Loading)
	double LoadingScreenHangDurationMultiplier = 1.0;

	// 即使在编辑器中也强制 Tick 加载界面
	// （在迭代加载界面时有用）
	UPROPERTY(config, EditAnywhere, Category=Loading)
	bool ForceTickLoadingScreenEvenInEditor = true;

	//~=========================================================================
	// 黑屏
	//~=========================================================================

	// 黑屏所使用的控件（系统级回退，在引擎过渡期间显示）。
	// 必须派生自 UBlackScreenUserWidget；留空时自动使用 UBlackScreenUserWidget 作为默认值。
	UPROPERTY(config, EditAnywhere, Category=BlackScreen, meta=(MetaClass="/Script/CommonLoadingScreen.BlackScreenUserWidget"))
	FSoftClassPath BlackScreenWidget;

	// 黑屏控件在视口堆栈中的 Z 顺序（默认低于 LoadingScreenZOrder）
	UPROPERTY(config, EditAnywhere, Category=BlackScreen)
	int32 BlackScreenZOrder = 9000;

	// 黑屏淡入动画时长（秒）
	UPROPERTY(config, EditAnywhere, Category=BlackScreen, meta=(ForceUnits=s))
	float BlackScreenFadeInDuration = 0.3f;

	// 黑屏淡出动画时长（秒）
	UPROPERTY(config, EditAnywhere, Category=BlackScreen, meta=(ForceUnits=s))
	float BlackScreenFadeOutDuration = 0.3f;

	// 黑屏消失后额外保持的时长（秒），以便给纹理流式加载留出时间，避免画面模糊
	UPROPERTY(config, EditAnywhere, Category=BlackScreen, meta=(ForceUnits=s))
	float HoldBlackScreenAdditionalSecs = 2.0f;

	// 即使在编辑器中也应用额外的 HoldBlackScreenAdditionalSecs 延迟
	UPROPERTY(config, EditAnywhere, Category=BlackScreen)
	bool HoldBlackScreenAdditionalSecsEvenInEditor = false;

	// 加载期间心跳挂起的倍率，放大心跳超时阈值以避免误判卡死。默认 1.0 表示不放大。
	UPROPERTY(config, EditAnywhere, Category=BlackScreen)
	double BlackScreenHangDurationMultiplier = 1.0;

	//~=========================================================================
	// 通用调试
	//~=========================================================================

	// 强制显示加载界面（用于调试）
	UPROPERTY(Transient, EditAnywhere, Category=Debug, meta=(ConsoleVariable="CommonLoadingScreen.AlwaysShow"))
	bool ForceLoadingScreenVisible = false;

	// 为 true 时，每帧都会将加载界面与黑屏的显示/隐藏原因输出到日志。
	UPROPERTY(Transient, EditAnywhere, Category=Debug, meta=(ConsoleVariable="CommonLoadingScreen.LogLoadingScreenReasonEveryFrame"))
	bool LogLoadingScreenReasonEveryFrame = 0;
};
