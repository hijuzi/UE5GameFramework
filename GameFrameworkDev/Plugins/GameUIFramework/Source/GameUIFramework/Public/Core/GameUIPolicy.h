// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"

#include "GameUIPolicy.generated.h"

#define UE_API GAMEUIFRAMEWORK_API

class UGameUIManagerBaseSubsystem;
class ULocalPlayer;
class UPrimaryGameUILayout;

/**
 * 本地多人交互模式枚举。
 */
UENUM()
enum class ELocalMultiplayerInteractionMode : uint8
{
	// 仅主玩家的全屏视口，无论其他玩家是否存在
	PrimaryOnly,

	// 一次仅一名玩家的全屏视口，但玩家可以切换控制权（谁显示/谁休眠）
	SingleToggle,

	// 两个玩家的视口同时显示
	Simultaneous
};

USTRUCT()
struct FRootViewportLayoutInfo
{
	GENERATED_BODY()
public:
	// 所属玩家
	UPROPERTY(Transient)
	TObjectPtr<ULocalPlayer> LocalPlayer = nullptr;

	// 玩家的 Layout 实例
	UPROPERTY(Transient)
	TObjectPtr<UPrimaryGameUILayout> RootLayout = nullptr;

	// 是否已添加到 Viewport
	UPROPERTY(Transient)
	bool bAddedToViewport = false;

	FRootViewportLayoutInfo() {}
	FRootViewportLayoutInfo(ULocalPlayer* InLocalPlayer, UPrimaryGameUILayout* InRootLayout, bool bIsInViewport)
		: LocalPlayer(InLocalPlayer)
		, RootLayout(InRootLayout)
		, bAddedToViewport(bIsInViewport)
	{}
	// 用于 TArray::FindByKey 查找
	bool operator==(const ULocalPlayer* OtherLocalPlayer) const { return LocalPlayer == OtherLocalPlayer; }
};

UCLASS(MinimalAPI, Abstract, Blueprintable, Within = GameUIManagerBaseSubsystem)
class UGameUIPolicy : public UObject
{
	GENERATED_BODY()

public:
	template <typename GameUIPolicyClass = UGameUIPolicy>
	static GameUIPolicyClass* GetGameUIPolicyAs(const UObject* WorldContextObject)
	{
		return Cast<GameUIPolicyClass>(GetGameUIPolicy(WorldContextObject));
	}

	static UE_API UGameUIPolicy* GetGameUIPolicy(const UObject* WorldContextObject);

public:
	UE_API virtual UWorld* GetWorld() const override;
	UE_API UGameUIManagerBaseSubsystem* GetOwningUIManager() const;
	UE_API UPrimaryGameUILayout* GetRootLayout(const ULocalPlayer* LocalPlayer) const;

	ELocalMultiplayerInteractionMode GetLocalMultiplayerInteractionMode() const { return LocalMultiplayerInteractionMode; }

	UE_API void RequestPrimaryControl(UPrimaryGameUILayout* Layout);

protected:
	UE_API void AddLayoutToViewport(ULocalPlayer* LocalPlayer, UPrimaryGameUILayout* Layout);
	UE_API void RemoveLayoutFromViewport(ULocalPlayer* LocalPlayer, UPrimaryGameUILayout* Layout);

	UE_API virtual void OnRootLayoutAddedToViewport(ULocalPlayer* LocalPlayer, UPrimaryGameUILayout* Layout);
	UE_API virtual void OnRootLayoutRemovedFromViewport(ULocalPlayer* LocalPlayer, UPrimaryGameUILayout* Layout);
	UE_API virtual void OnRootLayoutReleased(ULocalPlayer* LocalPlayer, UPrimaryGameUILayout* Layout);

	UE_API void CreateLayoutWidget(ULocalPlayer* LocalPlayer);
	UE_API TSubclassOf<UPrimaryGameUILayout> GetLayoutWidgetClass(ULocalPlayer* LocalPlayer);

private:
	ELocalMultiplayerInteractionMode LocalMultiplayerInteractionMode = ELocalMultiplayerInteractionMode::PrimaryOnly;

	UPROPERTY(EditAnywhere)
	TSoftClassPtr<UPrimaryGameUILayout> LayoutClass;

	// 每个玩家的 Layout 追踪表
	UPROPERTY(Transient)
	TArray<FRootViewportLayoutInfo> RootViewportLayouts;

private:
	UE_API void NotifyPlayerAdded(ULocalPlayer* LocalPlayer);
	UE_API void NotifyPlayerRemoved(ULocalPlayer* LocalPlayer);
	UE_API void NotifyPlayerDestroyed(ULocalPlayer* LocalPlayer);

	friend class UGameUIManagerBaseSubsystem;
};

#undef UE_API
