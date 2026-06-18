// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameCoreGameMode.h"
#include "GameFrameworkDevGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AGameFrameworkDevGameMode : public AGameCoreGameMode
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AGameFrameworkDevGameMode();
};



