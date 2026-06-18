// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVGameState.h"
#include "Component/SVExperienceManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SVGameState)

ASVGameState::ASVGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExperienceManagerComponent = CreateDefaultSubobject<USVExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));
}

void ASVGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}
