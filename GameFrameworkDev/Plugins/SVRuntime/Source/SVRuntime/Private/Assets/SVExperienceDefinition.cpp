// Copyright Epic Games, Inc. All Rights Reserved.

#include "Assets/SVExperienceDefinition.h"
#include "SVWorldSettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(SVExperienceDefinition)

#define LOCTEXT_NAMESPACE "SVSystem"

USVBaseExperienceDefinition::USVBaseExperienceDefinition()
{
}

#if WITH_EDITOR
EDataValidationResult USVBaseExperienceDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	return Result;
}
#endif

#if WITH_EDITORONLY_DATA
void USVBaseExperienceDefinition::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();
}
#endif

const USVBaseExperienceDefinition* USVBaseExperienceDefinition::GetCurrentExperienceDefinition(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	if (const ASVWorldSettings* WorldSettings = Cast<ASVWorldSettings>(World->GetWorldSettings()))
	{
		if (UClass* ExperienceClass = WorldSettings->GetDefaultGameplayExperienceSoftPtr().LoadSynchronous())
		{
			return GetDefault<USVBaseExperienceDefinition>(ExperienceClass);
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
