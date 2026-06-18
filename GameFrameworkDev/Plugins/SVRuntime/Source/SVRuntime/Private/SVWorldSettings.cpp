// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVWorldSettings.h"
#include "GameCoreGameMode.h"
#include "Misc/UObjectToken.h"
#include "Assets/SVExperienceDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SVWorldSettings)

ASVWorldSettings::ASVWorldSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultGameMode = AGameCoreGameMode::StaticClass();
}

FPrimaryAssetId ASVWorldSettings::GetDefaultGameplayExperience() const
{
	return DefaultGameplayExperience.ToSoftObjectPath().GetAssetPath().IsValid()
		? FPrimaryAssetId(FPrimaryAssetType(USVExperienceDefinition::StaticClass()->GetFName()),
				FName(*DefaultGameplayExperience.ToSoftObjectPath().GetAssetPath().GetAssetName().ToString()))
		: FPrimaryAssetId();
}

#if WITH_EDITOR
void ASVWorldSettings::CheckForErrors()
{
	Super::CheckForErrors();

	if (DefaultGameplayExperience.IsNull())
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::FromString(TEXT("SVWorldSettings: DefaultGameplayExperience is not set."))));
	}
}
#endif
