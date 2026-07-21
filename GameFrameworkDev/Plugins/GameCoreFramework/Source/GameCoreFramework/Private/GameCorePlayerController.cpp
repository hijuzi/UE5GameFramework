// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameCorePlayerController.h"
#include "GameCoreLocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameCorePlayerController)

AGameCorePlayerController::AGameCorePlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShowMouseCursor = true;
}

void AGameCorePlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	if (UGameCoreLocalPlayer* LocalPlayer = Cast<UGameCoreLocalPlayer>(Player))
	{
		LocalPlayer->OnPlayerControllerSet.Broadcast(LocalPlayer, this);
		
		if (PlayerState)
		{
			LocalPlayer->OnPlayerStateSet.Broadcast(LocalPlayer, PlayerState);
		}
	}
}

void AGameCorePlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	if (UGameCoreLocalPlayer* LocalPlayer = Cast<UGameCoreLocalPlayer>(Player))
	{
		LocalPlayer->OnPlayerPawnSet.Broadcast(LocalPlayer, InPawn);
	}
}

void AGameCorePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (UGameCoreLocalPlayer* LocalPlayer = Cast<UGameCoreLocalPlayer>(Player))
	{
		LocalPlayer->OnPlayerPawnSet.Broadcast(LocalPlayer, InPawn);
	}
}

void AGameCorePlayerController::OnUnPossess()
{
	Super::OnUnPossess();

	if (UGameCoreLocalPlayer* LocalPlayer = Cast<UGameCoreLocalPlayer>(Player))
	{
		LocalPlayer->OnPlayerPawnSet.Broadcast(LocalPlayer, nullptr);
	}
}

void AGameCorePlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (UGameCoreLocalPlayer* LocalPlayer = Cast<UGameCoreLocalPlayer>(Player))
	{
		LocalPlayer->OnPlayerStateSet.Broadcast(LocalPlayer, PlayerState);
	}
}