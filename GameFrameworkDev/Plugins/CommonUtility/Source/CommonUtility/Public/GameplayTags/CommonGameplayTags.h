// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * 项目中所有 GameplayTag 的统一入口。
 * 内部按类别拆分到独立文件管理（位于 Private/GameplayTags/ 下）：
 *   - GameUIGameplayTags.h    UI 相关 Tag
 *   - PlatformGameplayTags.h  Platform 相关 Tag
 *
 * 使用方式：直接引用 CommonGameplayTags::TAG_XXX 即可，无需魔法字符串。
 */
namespace CommonGameplayTags
{
	// ========================================================================
	//  UI Layer Tags（自上而下层级，用于 CommonUI 层级管理层）
	// ========================================================================

	COMMONUTILITY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GAMEUI_LAYER_MASK);
	COMMONUTILITY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GAMEUI_LAYER_WINDOW);
	COMMONUTILITY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GAMEUI_LAYER_FULLSCREENMENU);
	COMMONUTILITY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GAMEUI_LAYER_GAMEMENU);
	COMMONUTILITY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GAMEUI_LAYER_HUD);

	// ========================================================================
	//  UI Action Tags
	// ========================================================================

	COMMONUTILITY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GAMEUI_ACTION_ESCAPE);

	// ========================================================================
	//  Platform Tags
	// ========================================================================

	COMMONUTILITY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_PLATFORM_TRAIT_INPUT_PRIMARILYCONTROLLER);
};
