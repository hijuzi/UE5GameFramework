// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "UObject/SoftObjectPath.h"
#include "Framework/Application/IInputProcessor.h"

#include "LoadingScreenSettings.generated.h"

class UObject;

/**
 * 输入拦截器：加载界面显示期间吃掉所有输入。
 * 可供黑屏加载界面和关卡加载界面公用。
 */
class FLoadingScreenInputPreProcessor : public IInputProcessor
{
public:
	FLoadingScreenInputPreProcessor() {}
	virtual ~FLoadingScreenInputPreProcessor() {}

	bool CanEatInput() const
	{
		return !GIsEditor;
	}

	//~ IInputProcessor interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return CanEatInput(); }
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return CanEatInput(); }
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override { return CanEatInput(); }
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override { return CanEatInput(); }
	virtual bool HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& MotionEvent) override { return CanEatInput(); }
	//~ End of IInputProcessor interface
};

/**
 * 关卡加载动画过渡类型，决定关卡加载界面的显示/隐藏方式。
 */
UENUM(BlueprintType)
enum class ELoadingAnimationType : uint8
{
	Opacity		UMETA(DisplayName = "改变透明度"),
	Translation	UMETA(DisplayName = "移动位置"),
	Scale		UMETA(DisplayName = "缩放"),
};

/**
 * 关卡加载动画插值模式，控制动画曲线。
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
 * 关卡加载界面内容类型，决定关卡加载界面显示的背景内容。
 */
UENUM(BlueprintType)
enum class ELoadingScreenContentType : uint8
{
	Image		UMETA(DisplayName = "图片背景"),
	Video		UMETA(DisplayName = "视频"),
};

/**
 * 关卡加载界面系统的设置。
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Loading Screen"))
class ULoadingScreenSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	ULoadingScreenSettings();

public:
	//~=========================================================================
	// 关卡加载界面
	//~=========================================================================

	// 关卡加载界面所使用的控件，必须派生自 ULevelLoadingScreenWidget。
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen, meta=(MetaClass="/Script/LoadingScreenSystem.LevelLoadingScreenWidget"))
	FSoftClassPath LevelLoadingScreenWidget;

	// 关卡加载界面控件在视口堆栈中的 Z 顺序
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen)
	int32 LevelLoadingScreenZOrder = 9000;

	// 关卡加载界面内容类型
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen)
	ELoadingScreenContentType LevelLoadingScreenContentType = ELoadingScreenContentType::Image;

	// 图片背景资产（ContentType 为 Image 时生效）
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen, meta=(MetaClass="/Script/Engine.Texture2D"))
	FSoftObjectPath LevelLoadingScreenImageBackground;

	// 视频路径（ContentType 为 Video 时生效）
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen)
	FString LevelLoadingScreenVideoPath;

	// 关卡加载界面的最小显示时长（秒）。
	// 即使关卡加载已完成，也会至少显示这么长时间；
	// 若关卡加载未完成，则继续等待加载完成。
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen, meta=(ForceUnits=s, ClampMin="2.0", ClampMax="10.0"))
	float MinimumLevelLoadingScreenDisplayTime = 2.0f;

	// 其他加载完成后额外保持关卡加载界面的时长（秒），
	// 以便给纹理流式加载留出时间，避免画面模糊
	//
	// 注意：在编辑器中通常不应用此延迟，以加快迭代速度，但可以通过
	// HoldLevelLoadingScreenAdditionalSecsEvenInEditor 启用
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen, meta=(ForceUnits=s))
	float HoldLevelLoadingScreenAdditionalSecs = 2.0f;

	// 即使在编辑器中也应用额外的 HoldLevelLoadingScreenAdditionalSecs 延迟
	// （在迭代关卡加载界面时有用）
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen)
	bool HoldLevelLoadingScreenAdditionalSecsEvenInEditor = false;

	// 超过此秒数（非零时）后关卡加载界面被视为永久挂起。
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen, meta=(ForceUnits=s))
	float LevelLoadingScreenHeartbeatHangDuration = 0.0f;

	// 每隔多少秒输出一次保持关卡加载界面的日志（非零时）。
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen, meta=(ForceUnits=s))
	float LogLevelLoadingScreenHeartbeatInterval = 5.0f;

	// 加载期间心跳挂起的倍率，放大心跳超时阈值以避免误判卡死。默认 1.0 表示不放大。
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen)
	double LevelLoadingScreenHangDurationMultiplier = 1.0;

	// 关卡加载界面白名单检测帧数，每隔 N 帧检查一次是否需要隐藏关卡加载界面
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen, meta=(ClampMin=1, ClampMax=100))
	int32 LevelLoadingScreenWhitelistCheckFrames = 10;

	//~=========================================================================
	// 黑屏加载界面
	//~=========================================================================

	// 黑屏加载界面所使用的控件（系统级回退，在引擎过渡期间显示）。
	UPROPERTY(config, EditAnywhere, Category=BlackLoadingScreen, meta=(MetaClass="/Script/LoadingScreenSystem.BlackLoadingScreenWidget"))
	FSoftClassPath BlackLoadingScreenWidget;

	// 黑屏加载界面控件在视口堆栈中的 Z 顺序（游戏中最高层级，默认高于关卡加载界面）
	UPROPERTY(config, EditAnywhere, Category=BlackLoadingScreen)
	int32 BlackLoadingScreenZOrder = 10000;

	// 黑屏加载界面加载时长（秒），淡入动画时长
	UPROPERTY(config, EditAnywhere, Category=BlackLoadingScreen, meta=(ForceUnits=s))
	float BlackLoadingScreenLoadDuration = 0.5f;

	// 黑屏加载界面卸载时长（秒），淡出动画时长
	UPROPERTY(config, EditAnywhere, Category=BlackLoadingScreen, meta=(ForceUnits=s))
	float BlackLoadingScreenUnloadDuration = 0.5f;

	// 黑屏加载界面动画过渡类型
	UPROPERTY(config, EditAnywhere, Category=BlackLoadingScreen)
	ELoadingAnimationType BlackLoadingScreenAnimationType = ELoadingAnimationType::Opacity;

	// 黑屏加载界面动画插值模式
	UPROPERTY(config, EditAnywhere, Category=BlackLoadingScreen)
	ELoadingAnimationMode BlackLoadingScreenAnimationMode = ELoadingAnimationMode::Linear;

	// 黑屏加载界面消失后额外保持的时长（秒），以便给纹理流式加载留出时间，避免画面模糊
	UPROPERTY(config, EditAnywhere, Category=BlackLoadingScreen, meta=(ForceUnits=s))
	float HoldBlackLoadingScreenAdditionalSecs = 2.0f;

	// 即使在编辑器中也应用额外的 HoldBlackLoadingScreenAdditionalSecs 延迟
	UPROPERTY(config, EditAnywhere, Category=BlackLoadingScreen)
	bool HoldBlackLoadingScreenAdditionalSecsEvenInEditor = false;

	// 加载期间心跳挂起的倍率，放大心跳超时阈值以避免误判卡死。默认 1.0 表示不放大。
	UPROPERTY(config, EditAnywhere, Category=BlackLoadingScreen)
	double BlackLoadingScreenHangDurationMultiplier = 1.0;

	//~=========================================================================
	// 通用调试
	//~=========================================================================

	// 强制显示关卡加载界面（用于调试）
	UPROPERTY(config, EditAnywhere, Category=Debug)
	bool bForceLevelLoadingScreenVisible = false;

	// 为 true 时，每帧都会将关卡加载界面与黑屏加载界面的显示/隐藏原因输出到日志。
	UPROPERTY(config, EditAnywhere, Category=Debug)
	bool bLogLoadingScreenReasonEveryFrame = false;

	// 即使在编辑器中也强制 Tick 关卡加载界面
	// （在迭代关卡加载界面时有用）
	UPROPERTY(config, EditAnywhere, Category=Debug)
	bool ForceTickLoadingScreenEvenInEditor = true;
};
