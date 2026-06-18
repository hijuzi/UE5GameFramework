// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/GameUIPolicy.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Core/GameUIManagerBaseSubsystem.h"
#include "GameCoreLocalPlayer.h"
#include "Core/PrimaryGameUILayout.h"
#include "Engine/Engine.h"
#include "LogGameUIFramework.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameUIPolicy)

// 静态方法
UGameUIPolicy* UGameUIPolicy::GetGameUIPolicy(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UGameUIManagerBaseSubsystem* UIManager = UGameInstance::GetSubsystem<UGameUIManagerBaseSubsystem>(GameInstance))
			{
				return UIManager->GetCurrentUIPolicy();
			}
		}
	}

	return nullptr;
}

UGameUIManagerBaseSubsystem* UGameUIPolicy::GetOwningUIManager() const
{
	return CastChecked<UGameUIManagerBaseSubsystem>(GetOuter());
}

UWorld* UGameUIPolicy::GetWorld() const
{
	return GetOwningUIManager()->GetGameInstance()->GetWorld();
}

UPrimaryGameUILayout* UGameUIPolicy::GetRootLayout(const ULocalPlayer* LocalPlayer) const
{
	const FRootViewportLayoutInfo* LayoutInfo = RootViewportLayouts.FindByKey(LocalPlayer);
	return LayoutInfo ? LayoutInfo->RootLayout : nullptr;
}

void UGameUIPolicy::NotifyPlayerAdded(ULocalPlayer* LocalPlayer)
{
	// 绑定 OnPlayerControllerSet 委托：当 PC 生成后重新创建/添加 Layout
	if (UGameCoreLocalPlayer* GameCoreLocalPlayer = Cast<UGameCoreLocalPlayer>(LocalPlayer))
	{
		GameCoreLocalPlayer->OnPlayerControllerSet.AddWeakLambda(this, [this](UGameCoreLocalPlayer* CommonLocalPlayer, APlayerController* PlayerController)
		{
			NotifyPlayerRemoved(CommonLocalPlayer);

			if (FRootViewportLayoutInfo* LayoutInfo = RootViewportLayouts.FindByKey(CommonLocalPlayer))
			{
				AddLayoutToViewport(CommonLocalPlayer, LayoutInfo->RootLayout);
				LayoutInfo->bAddedToViewport = true;
			}
			else
			{
				CreateLayoutWidget(CommonLocalPlayer);
			}
		});
	}

	if (FRootViewportLayoutInfo* LayoutInfo = RootViewportLayouts.FindByKey(LocalPlayer))
	{
		AddLayoutToViewport(LocalPlayer, LayoutInfo->RootLayout);
		LayoutInfo->bAddedToViewport = true;
	}
	else
	{
		CreateLayoutWidget(LocalPlayer);
	}
}

void UGameUIPolicy::NotifyPlayerRemoved(ULocalPlayer* LocalPlayer)
{
	if (FRootViewportLayoutInfo* LayoutInfo = RootViewportLayouts.FindByKey(LocalPlayer))
	{
		RemoveLayoutFromViewport(LocalPlayer, LayoutInfo->RootLayout);
		LayoutInfo->bAddedToViewport = false;

		// SingleToggle 模式特殊处理：次要玩家离开时交还控制权
		if (LocalMultiplayerInteractionMode == ELocalMultiplayerInteractionMode::SingleToggle && LocalPlayer->GetControllerId() != 0)
		{
			UPrimaryGameUILayout* RootLayout = LayoutInfo->RootLayout;
			if (RootLayout && !RootLayout->IsDormant())
			{
				// 次要玩家 Layout 休眠
				RootLayout->SetIsDormant(true);
				for (const FRootViewportLayoutInfo& RootLayoutInfo : RootViewportLayouts)
				{
					if (RootLayoutInfo.LocalPlayer->GetControllerId() == 0)
					{
						if (UPrimaryGameUILayout* PrimaryRootLayout = RootLayoutInfo.RootLayout)
						{
							// 主玩家 Layout 唤醒
							PrimaryRootLayout->SetIsDormant(false);
						}
					}
				}
			}
		}
	}
}

void UGameUIPolicy::NotifyPlayerDestroyed(ULocalPlayer* LocalPlayer)
{
	NotifyPlayerRemoved(LocalPlayer);

	// 清理 OnPlayerControllerSet 绑定
	if (UGameCoreLocalPlayer* GameCoreLocalPlayer = Cast<UGameCoreLocalPlayer>(LocalPlayer))
	{
		GameCoreLocalPlayer->OnPlayerControllerSet.RemoveAll(this);
	}

	const int32 LayoutInfoIdx = RootViewportLayouts.IndexOfByKey(LocalPlayer);
	if (LayoutInfoIdx != INDEX_NONE)
	{
		UPrimaryGameUILayout* Layout = RootViewportLayouts[LayoutInfoIdx].RootLayout;
		RootViewportLayouts.RemoveAt(LayoutInfoIdx);

		RemoveLayoutFromViewport(LocalPlayer, Layout);

		OnRootLayoutReleased(LocalPlayer, Layout);
	}
}

void UGameUIPolicy::AddLayoutToViewport(ULocalPlayer* LocalPlayer, UPrimaryGameUILayout* Layout)
{
	UE_LOG(LogGameUIFramework, Log, TEXT("[%s] is adding player [%s]'s root layout [%s] to the viewport"), *GetName(), *GetNameSafe(LocalPlayer), *GetNameSafe(Layout));

	Layout->SetPlayerContext(FLocalPlayerContext(LocalPlayer));
	// ZOrder = 1000，添加到视口最上层
	Layout->AddToPlayerScreen(1000);

	OnRootLayoutAddedToViewport(LocalPlayer, Layout);
}

void UGameUIPolicy::RemoveLayoutFromViewport(ULocalPlayer* LocalPlayer, UPrimaryGameUILayout* Layout)
{
	TWeakPtr<SWidget> LayoutSlateWidget = Layout->GetCachedWidget();
	if (LayoutSlateWidget.IsValid())
	{
		UE_LOG(LogGameUIFramework, Log, TEXT("[%s] is removing player [%s]'s root layout [%s] from the viewport"), *GetName(), *GetNameSafe(LocalPlayer), *GetNameSafe(Layout));

		// 从 Slate 层级移除
		Layout->RemoveFromParent();
		if (LayoutSlateWidget.IsValid())
		{
			UE_LOG(LogGameUIFramework, Log, TEXT("Player [%s]'s root layout [%s] has been removed from the viewport, but other references to its underlying Slate widget still exist. Noting in case we leak it."), *GetNameSafe(LocalPlayer), *GetNameSafe(Layout));
		}

		OnRootLayoutRemovedFromViewport(LocalPlayer, Layout);
	}
}

void UGameUIPolicy::OnRootLayoutAddedToViewport(ULocalPlayer* LocalPlayer, UPrimaryGameUILayout* Layout)
{
#if WITH_EDITOR
	if (GIsEditor && LocalPlayer->GetControllerId() == 0)
	{
		// 使我们的控制器在 PIE 中无需点击视口即可工作
		FSlateApplication::Get().SetUserFocusToGameViewport(0);
	}
#endif
}

void UGameUIPolicy::OnRootLayoutRemovedFromViewport(ULocalPlayer* LocalPlayer, UPrimaryGameUILayout* Layout)
{
	
}

void UGameUIPolicy::OnRootLayoutReleased(ULocalPlayer* LocalPlayer, UPrimaryGameUILayout* Layout)
{
	
}

void UGameUIPolicy::RequestPrimaryControl(UPrimaryGameUILayout* Layout)
{
	if (LocalMultiplayerInteractionMode == ELocalMultiplayerInteractionMode::SingleToggle && Layout->IsDormant())
	{
		// Step 1: 让当前活跃的 layout 休眠
		for (const FRootViewportLayoutInfo& LayoutInfo : RootViewportLayouts)
		{
			UPrimaryGameUILayout* RootLayout = LayoutInfo.RootLayout;
			if (RootLayout && !RootLayout->IsDormant())
			{
				RootLayout->SetIsDormant(true);
				break;
			}
		}
		// Step 2: 激活请求的 layout
		Layout->SetIsDormant(false);
	}
}

void UGameUIPolicy::CreateLayoutWidget(ULocalPlayer* LocalPlayer)
{
	if (APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld()))
	{
		// 1. 获取 Layout 蓝图类（同步加载 TSoftClassPtr）
		TSubclassOf<UPrimaryGameUILayout> LayoutWidgetClass = GetLayoutWidgetClass(LocalPlayer);
		if (ensure(LayoutWidgetClass && !LayoutWidgetClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			// 2. 创建 Widget 实例（Owner = PlayerController）
			UPrimaryGameUILayout* NewLayoutObject = CreateWidget<UPrimaryGameUILayout>(PlayerController, LayoutWidgetClass);
			
			// 3. 记录到追踪表（bAddedToViewport 预置为 true）
			RootViewportLayouts.Emplace(LocalPlayer, NewLayoutObject, true);
			
			// 4. 添加到 Viewport
			AddLayoutToViewport(LocalPlayer, NewLayoutObject);
		}
	}
}

TSubclassOf<UPrimaryGameUILayout> UGameUIPolicy::GetLayoutWidgetClass(ULocalPlayer* LocalPlayer)
{
	return LayoutClass.LoadSynchronous();
}
