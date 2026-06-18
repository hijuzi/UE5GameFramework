// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameCoreGameInstance.h"
#include "Engine/LocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameCoreGameInstance)

UGameCoreGameInstance::UGameCoreGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

ULocalPlayer* UGameCoreGameInstance::GetPrimaryPlayer() const
{
	return PrimaryPlayer.Get();
}

int32 UGameCoreGameInstance::AddLocalPlayer(ULocalPlayer* NewPlayer, FPlatformUserId UserId)
{
	const int32 ReturnVal = Super::AddLocalPlayer(NewPlayer, UserId);
	if (ReturnVal != INDEX_NONE)
	{
		if (!PrimaryPlayer.IsValid())
		{
			PrimaryPlayer = MakeWeakObjectPtr(NewPlayer);
		}

		OnPlayerAdded.Broadcast(NewPlayer);
	}

	return ReturnVal;
}

bool UGameCoreGameInstance::RemoveLocalPlayer(ULocalPlayer* ExistingPlayer)
{
	if (PrimaryPlayer.Get() == ExistingPlayer)
	{
		PrimaryPlayer.Reset();
	}

	OnPlayerDestroyed.Broadcast(ExistingPlayer);

	return Super::RemoveLocalPlayer(ExistingPlayer);
}
