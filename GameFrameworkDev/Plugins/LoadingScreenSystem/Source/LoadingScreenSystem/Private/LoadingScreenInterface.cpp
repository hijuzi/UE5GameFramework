// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingScreenInterface.h"

bool ILevelLoadingScreenInterface::ShouldShowLevelLoadingScreen(UObject* TestObject, FString& OutReason)
{
	if (!TestObject)
	{
		return false;
	}

	if (const ILevelLoadingScreenInterface* Interface = Cast<ILevelLoadingScreenInterface>(TestObject))
	{
		return Interface->ShouldShowLevelLoadingScreen(OutReason);
	}

	return false;
}
