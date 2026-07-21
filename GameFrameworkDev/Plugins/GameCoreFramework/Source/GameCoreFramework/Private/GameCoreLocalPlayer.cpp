// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameCoreLocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameCoreLocalPlayer)

UGameCoreLocalPlayer::UGameCoreLocalPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FDelegateHandle UGameCoreLocalPlayer::CallAndRegister_OnPlayerControllerSet(FPlayerControllerSetDelegate::FDelegate Delegate)
{
	if (APlayerController* PC = GetPlayerController(GetWorld()))
	{
		Delegate.Execute(this, PC);
	}
	return OnPlayerControllerSet.Add(Delegate);
}

FDelegateHandle UGameCoreLocalPlayer::CallAndRegister_OnPlayerStateSet(FPlayerStateSetDelegate::FDelegate Delegate)
{
	if (APlayerController* PC = GetPlayerController(GetWorld()))
	{
		if (APlayerState* PS = PC->PlayerState)
		{
			Delegate.Execute(this, PS);
		}
	}
	return OnPlayerStateSet.Add(Delegate);
}

FDelegateHandle UGameCoreLocalPlayer::CallAndRegister_OnPlayerPawnSet(FPlayerPawnSetDelegate::FDelegate Delegate)
{
	if (APlayerController* PC = GetPlayerController(GetWorld()))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Delegate.Execute(this, Pawn);
		}
	}
	return OnPlayerPawnSet.Add(Delegate);
}
