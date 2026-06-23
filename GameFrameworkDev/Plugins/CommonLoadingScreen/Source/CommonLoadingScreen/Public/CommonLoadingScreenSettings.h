// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "UObject/SoftObjectPath.h"

#include "CommonLoadingScreenSettings.generated.h"

class UObject;

/**
 * 缓动类型，用于加载界面和黑屏的淡入淡出动画。
 */
UENUM(BlueprintType)
enum class EFadeEasing : uint8
{
	None		UMETA(DisplayName = "无"),
	EaseIn		UMETA(DisplayName = "缓入"),
	EaseOut		UMETA(DisplayName = "缓出"),
};

/**
 * 加载动画过渡类型，决定加载界面和黑屏的显示/隐藏方式。
 */
UENUM(BlueprintType)
enum class ELoadingAnimationType : uint8
{
	Opacity		UMETA(DisplayName = "改变透明度"),
	Translation	UMETA(DisplayName = "移动位置"),
	Scale		UMETA(DisplayName = "缩放"),
};

/**
 * 加载动画插值模式，控制动画曲线。
 */
UENUM(BlueprintType)
enum class ELoadingAnimationMode : uint8
{
	Linear		UMETA(DisplayName = "线性"),
	Sine		UMETA(DisplayName = "正弦"),
	Quadratic	UMETA(DisplayName = "二次方"),
	Cubic		UMETA(DisplayName = "立方"),
};

/**
 * 加载界面内容类型，决定加载界面显示的背景内容。
 */
UENUM(BlueprintType)
enum class ELoadingScreenContentType : uint8
{
	Image		UMETA(DisplayName = "图片背景"),
	Video		UMETA(DisplayName = "视频"),
};

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

	// 加载界面所使用的控件，必须派生自 ULoadingProgressUserWidget。
	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(MetaClass="/Script/CommonLoadingScreen.LoadingProgressUserWidget"))
	FSoftClassPath LoadingScreenWidget;

	// 加载界面控件在视口堆栈中的 Z 顺序
	UPROPERTY(config, EditAnywhere, Category=Loading)
	int32 LoadingScreenZOrder = 10000;

	// 加载界面加载时长（秒），淡入动画时长
	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(ForceUnits=s))
	float LoadingScreenLoadDuration = 0.5f;

	// 加载界面卸载时长（秒），淡出动画时长
	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(ForceUnits=s))
	float LoadingScreenUnloadDuration = 0.5f;

	// 加载界面动画过渡类型
	UPROPERTY(config, EditAnywhere, Category=Loading)
	ELoadingAnimationType LoadingScreenAnimationType = ELoadingAnimationType::Opacity;

	// 加载界面动画插值模式
	UPROPERTY(config, EditAnywhere, Category=Loading)
	ELoadingAnimationMode LoadingScreenAnimationMode = ELoadingAnimationMode::Linear;

	// 加载界面内容类型
	UPROPERTY(config, EditAnywhere, Category=Loading)
	ELoadingScreenContentType LoadingScreenContentType = ELoadingScreenContentType::Image;

	// 图片背景资产（ContentType 为 Image 时生效）
	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(MetaClass="/Script/Engine.Texture2D"))
	FSoftObjectPath LoadingScreenImageBackground;

	// 视频路径（ContentType 为 Video 时生效）
	UPROPERTY(config, EditAnywhere, Category=Loading)
	FString LoadingScreenVideoPath;

	// 加载界面的最小显示时长（秒）。
	// 即使关卡加载已完成，也会至少显示这么长时间；
	// 若关卡加载未完成，则继续等待加载完成。
	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(ForceUnits=s, ClampMin="2.0", ClampMax="10.0"))
	float MinimumLoadingScreenDisplayTime = 2.0f;

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

	// 加载界面白名单检测帧数，每隔 N 帧检查一次是否需要隐藏加载界面
	UPROPERTY(config, EditAnywhere, Category=Loading, meta=(ClampMin=1, ClampMax=100))
	int32 LoadingScreenWhitelistCheckFrames = 10;

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

	// 黑屏加载时长（秒），淡入动画时长
	UPROPERTY(config, EditAnywhere, Category=BlackScreen, meta=(ForceUnits=s))
	float BlackScreenLoadDuration = 0.5f;

	// 黑屏卸载时长（秒），淡出动画时长
	UPROPERTY(config, EditAnywhere, Category=BlackScreen, meta=(ForceUnits=s))
	float BlackScreenUnloadDuration = 0.5f;

	// 黑屏动画过渡类型
	UPROPERTY(config, EditAnywhere, Category=BlackScreen)
	ELoadingAnimationType BlackScreenAnimationType = ELoadingAnimationType::Opacity;

	// 黑屏动画插值模式
	UPROPERTY(config, EditAnywhere, Category=BlackScreen)
	ELoadingAnimationMode BlackScreenAnimationMode = ELoadingAnimationMode::Linear;

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
	UPROPERTY(config, EditAnywhere, Category=Debug)
	bool bForceLoadingScreenVisible = false;

	// 为 true 时，每帧都会将加载界面与黑屏的显示/隐藏原因输出到日志。
	UPROPERTY(config, EditAnywhere, Category=Debug)
	bool bLogLoadingScreenReasonEveryFrame = false;
};
