// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "LoadingScreenInputPreProcessor.h"

#include "LoadingScreenSettings.generated.h"

class UObject;

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
 * 关卡加载界面覆盖参数结构体
 * 用于 DataTable 行中按关卡覆盖加载界面的显示内容和参数。
 */
USTRUCT(BlueprintType)
struct LOADINGSCREENSYSTEM_API FLevelLoadingScreenOverrideConfig
{
	GENERATED_BODY()

	// ---- 覆盖开关 ----

	/** 是否覆盖 Content 参数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
	bool bOverrideContent = false;

	// ---- Content 参数 ----

	/** 关卡加载界面内容类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
	ELoadingScreenContentType ContentType = ELoadingScreenContentType::Image;

	/** 图片背景资产（ContentType 为 Image 时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
	FSoftObjectPath ImageBackground;

	/** 视频路径（ContentType 为 Video 时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
	FString VideoPath;
};

/**
 * DataTable 行结构体：关卡加载界面配置表。
 * 每行对应一个关卡，控制该关卡是否显示加载界面及对应的覆盖参数。
 */
USTRUCT(BlueprintType)
struct LOADINGSCREENSYSTEM_API FLevelLoadingScreenTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 关卡/World 路径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath LevelMap;

	/** 是否显示关卡加载界面。未在表中配置的关卡默认为显示（true）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	bool bShouldShowLevelLoadingScreen = false;

	/** 关卡加载界面覆盖配置（视频、图片等） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
	FLevelLoadingScreenOverrideConfig OverrideConfig;
};

/**
 * 关卡加载界面系统的设置。
 */
UCLASS(config=LoadingScreenSystem, defaultconfig, meta=(DisplayName="Loading Screen"))
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


	// 即使在编辑器中也应用额外的 HoldLevelLoadingScreenAdditionalSecs 延迟
	// （在迭代关卡加载界面时有用）
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen)
	bool HoldLevelLoadingScreenAdditionalSecsEvenInEditor = false;

	// 超过此秒数（非零时）后关卡加载界面被视为永久挂起。
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen, meta=(ForceUnits=s))
	float LevelLoadingScreenHeartbeatHangDuration = 0.0f;

	// 加载期间心跳挂起的倍率，放大心跳超时阈值以避免误判卡死。默认 1.0 表示不放大。
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen)
	double LevelLoadingScreenHangDurationMultiplier = 1.0;

	// 关卡加载界面覆盖配置表（DataTable），用于按关卡控制是否显示加载界面及覆盖参数。
	// 行结构体：FLevelLoadingScreenTableRow
	// 若未配置 / 表中未找到当前关卡，则默认显示加载界面。
	UPROPERTY(config, EditAnywhere, Category=LevelLoadingScreen, meta=(AllowedClasses="/Script/Engine.DataTable"))
	FSoftObjectPath LevelLoadingScreenOverrideTable;

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
	float BlackLoadingScreenLoadDuration = 0.15f;

	// 黑屏加载界面卸载时长（秒），淡出动画时长
	UPROPERTY(config, EditAnywhere, Category=BlackLoadingScreen, meta=(ForceUnits=s))
	float BlackLoadingScreenUnloadDuration = 1.0f;

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
	bool ForceTickLoadingScreenEvenInEditor = false;
};
