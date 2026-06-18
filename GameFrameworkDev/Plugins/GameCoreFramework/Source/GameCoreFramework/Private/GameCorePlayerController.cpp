// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameCorePlayerController.h"
#include "GameCoreLocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameCorePlayerController)

AGameCorePlayerController::AGameCorePlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AGameCorePlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	if (UGameCoreLocalPlayer* LocalPlayer = Cast<UGameCoreLocalPlayer>(Player))
	{
		LocalPlayer->OnPlayerControllerSet.Broadcast(LocalPlayer, this);
	}
}
