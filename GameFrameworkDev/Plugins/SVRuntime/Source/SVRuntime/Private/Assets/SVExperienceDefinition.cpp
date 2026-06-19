// Copyright Epic Games, Inc. All Rights Reserved.

#include "Assets/SVExperienceDefinition.h"

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

#undef LOCTEXT_NAMESPACE
