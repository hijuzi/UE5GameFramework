// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameCoreHUD.h"
#include "GameFramework/GameModeBase.h"
#include "GameFrameworkDevGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AGameFrameworkDevGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AGameFrameworkDevGameMode();
};



