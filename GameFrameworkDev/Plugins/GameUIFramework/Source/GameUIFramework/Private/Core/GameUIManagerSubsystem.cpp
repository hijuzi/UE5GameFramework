// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/GameUIManagerSubsystem.h"

#include "Engine/GameInstance.h"
#include "GameFramework/HUD.h"
#include "Core/GameUIPolicy.h"
#include "Core/PrimaryGameUILayout.h"
#include "GameCoreGameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameUIManagerSubsystem)

class FSubsystemCollectionBase;

UGameUIManagerSubsystem::UGameUIManagerSubsystem()
{
}

void UGameUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGameCoreGameInstance* GI = Cast<UGameCoreGameInstance>(GetGameInstance()))
	{
		GI->OnPlayerAdded.AddUObject(this, &ThisClass::NotifyPlayerAdded);
		GI->OnPlayerDestroyed.AddUObject(this, &ThisClass::NotifyPlayerDestroyed);
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGameUIManagerSubsystem::Tick), 0.0f);
}

void UGameUIManagerSubsystem::Deinitialize()
{
	if (UGameCoreGameInstance* GI = Cast<UGameCoreGameInstance>(GetGameInstance()))
	{
		GI->OnPlayerAdded.RemoveAll(this);
		GI->OnPlayerDestroyed.RemoveAll(this);
	}

	Super::Deinitialize();

	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
}

bool UGameUIManagerSubsystem::Tick(float DeltaTime)
{
	SyncRootLayoutVisibilityToShowHUD();
	
	return true;
}

void UGameUIManagerSubsystem::SyncRootLayoutVisibilityToShowHUD()
{
	if (const UGameUIPolicy* Policy = GetCurrentUIPolicy())
	{
		for (const ULocalPlayer* LocalPlayer : GetGameInstance()->GetLocalPlayers())
		{
			bool bShouldShowUI = true;
			
			if (const APlayerController* PC = LocalPlayer->GetPlayerController(GetWorld()))
			{
				const AHUD* HUD = PC->GetHUD();

				if (HUD && !HUD->bShowHUD)
				{
					bShouldShowUI = false;
				}
			}

			if (UPrimaryGameUILayout* RootLayout = Policy->GetRootLayout(LocalPlayer))
			{
				const ESlateVisibility DesiredVisibility = bShouldShowUI ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
				if (DesiredVisibility != RootLayout->GetVisibility())
				{
					RootLayout->SetVisibility(DesiredVisibility);	
				}
			}
		}
	}
}
