// Copyright xiele. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PseudoRandomTestKey.generated.h"

/**
 * 伪随机测试用 Key（具体 UObject 子类）
 *
 * 仅作为 UPRBPseudoRandomSubsystem 的 Object Key 使用，无额外逻辑。
 * 不能直接 NewObject<UObject>()，因为 UE5 中 UObject 为抽象类。
 */
UCLASS(BlueprintType)
class TESTUTILITY_API UPseudoRandomTestKey : public UObject
{
	GENERATED_BODY()
};
