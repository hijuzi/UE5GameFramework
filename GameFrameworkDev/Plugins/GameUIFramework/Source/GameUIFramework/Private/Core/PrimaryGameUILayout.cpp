// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/PrimaryGameUILayout.h"

#include "Engine/GameInstance.h"
#include "Core/GameUIManagerBaseSubsystem.h"
#include "Core/GameUIPolicy.h"
#include "Kismet/GameplayStatics.h"
#include "LogGameUIFramework.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PrimaryGameUILayout)

class UObject;

/*static*/ UPrimaryGameUILayout* UPrimaryGameUILayout::GetPrimaryGameLayoutForPrimaryPlayer(const UObject* WorldContextObject)
{
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	APlayerController* PlayerController = GameInstance->GetPrimaryPlayerController(false);
	return GetPrimaryGameLayout(PlayerController);
}

/*static*/ UPrimaryGameUILayout* UPrimaryGameUILayout::GetPrimaryGameLayout(APlayerController* PlayerController)
{
	return PlayerController ? GetPrimaryGameLayout(PlayerController->GetLocalPlayer()) : nullptr;
}

/*static*/ UPrimaryGameUILayout* UPrimaryGameUILayout::GetPrimaryGameLayout(ULocalPlayer* LocalPlayer)
{
	if (LocalPlayer)
	{
		if (const UGameInstance* GameInstance = LocalPlayer->GetGameInstance())
		{
			if (UGameUIManagerBaseSubsystem* UIManager = GameInstance->GetSubsystem<UGameUIManagerBaseSubsystem>())
			{
				if (const UGameUIPolicy* Policy = UIManager->GetCurrentUIPolicy())
				{
					if (UPrimaryGameUILayout* RootLayout = Policy->GetRootLayout(LocalPlayer))
					{
						return RootLayout;
					}
				}
			}
		}
	}

	return nullptr;
}

UPrimaryGameUILayout::UPrimaryGameUILayout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPrimaryGameUILayout::SetIsDormant(bool InDormant)
{
	if (bIsDormant != InDormant)
	{
		const ULocalPlayer* LP = GetOwningLocalPlayer();
		const int32 PlayerId = LP ? LP->GetControllerId() : -1;
		const TCHAR* OldDormancyStr = bIsDormant ? TEXT("Dormant") : TEXT("Not-Dormant");
		const TCHAR* NewDormancyStr = InDormant ? TEXT("Dormant") : TEXT("Not-Dormant");
		UE_LOG(LogGameUIFramework, Display, TEXT("PrimaryGameUILayout Dormancy changed for [%d] from [%s] to [%s]"), PlayerId, OldDormancyStr, NewDormancyStr);

		bIsDormant = InDormant;
		OnIsDormantChanged();
	}
}

void UPrimaryGameUILayout::OnIsDormantChanged()
{
	//@TODO NDarnell 决定如何处理休眠状态——过去我们将休眠作为一种方式，在强制多玩家使用单玩家视图时
	//关闭其他本地玩家的渲染和视图。
	
	//if (UCommonLocalPlayer* LocalPlayer = GetOwningLocalPlayer<UCommonLocalPlayer>())
	//{
	//	// 当根布局处于休眠状态时，我们也不想渲染来自所有者视图的任何内容
	//	LocalPlayer->SetIsPlayerViewEnabled(!bIsDormant);
	//}

	//SetVisibility(bIsDormant ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	//OnLayoutDormancyChanged().Broadcast(bIsDormant);
}

void UPrimaryGameUILayout::RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerWidget)
{
	if (!IsDesignTime())
	{
		// 监听 Layer 切换事件，自动挂起/恢复输入
		LayerWidget->OnTransitioningChanged.AddUObject(this,
			&UPrimaryGameUILayout::OnWidgetStackTransitioning);
		// 过渡时长设为 0（避免手柄焦点错乱）
		LayerWidget->SetTransitionDuration(0.0);
		// 注册到 Tag→Layer 映射
		Layers.Add(LayerTag, LayerWidget);
	}
}

void UPrimaryGameUILayout::OnWidgetStackTransitioning(UCommonActivatableWidgetContainerBase* Widget, bool bIsTransitioning)
{
	if (bIsTransitioning)
	{
		// 过渡开始 → 挂起输入（防误触）
		const FName SuspendToken = UGameUIExtensions::SuspendInputForPlayer(GetOwningLocalPlayer(), TEXT("GlobalStackTransion"));
		SuspendInputTokens.Add(SuspendToken);
	}
	else
	{
		if (ensure(SuspendInputTokens.Num() > 0))
		{
			// 过渡完成 → 恢复输入
			const FName SuspendToken = SuspendInputTokens.Pop();
			UGameUIExtensions::ResumeInputForPlayer(GetOwningLocalPlayer(), SuspendToken);
		}
	}
}

void UPrimaryGameUILayout::FindAndRemoveWidgetFromLayer(UCommonActivatableWidget* ActivatableWidget)
{
	// 不确定控件在哪个层上，所以逐个搜索。
	for (const auto& Layer : Layers)
	{
		Layer.Value->RemoveWidget(*ActivatableWidget);
	}
}

UCommonActivatableWidgetContainerBase* UPrimaryGameUILayout::GetLayerWidget(FGameplayTag LayerName)
{
	return Layers.FindRef(LayerName);
}
