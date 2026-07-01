// Copyright xiele. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PseudoRandom/PseudoRandomTestTypes.h"
#include "PseudoRandomTestRunner.generated.h"

/**
 * PRB 伪随机测试运行器（蓝图函数库）
 *
 * 作为 TestUtility 插件的蓝图可调用测试入口：
 *   - UObject Key 测试：自动 NewObject<UObject>() 作为 Key
 *   - GameplayTag Key 测试：自动创建 Random.Pseudo.Test.* Tag 作为 Key
 *
 * 每类测试覆盖 21 个概率等级（1%~95%）× 10 个实例 × 1000 次判定，
 * 输出 Summary + Detail CSV 到 ProjectSaved/PseudoRandomTests/。
 */
UCLASS()
class TESTUTILITY_API UPseudoRandomTestRunner : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** UObject Key 伪随机测试：多概率多实例，输出 Summary + Detail CSV */
	UFUNCTION(BlueprintCallable, Category = "PseudoRandom|Test", meta = (WorldContext = "WorldContextObject"))
	static void RunObjectPseudoRandomTest(const UObject* WorldContextObject, FPseudoRandomTestConfig Config);

	/** GameplayTag Key 伪随机测试：多概率多实例，输出 Summary + Detail CSV */
	UFUNCTION(BlueprintCallable, Category = "PseudoRandom|Test", meta = (WorldContext = "WorldContextObject"))
	static void RunTagPseudoRandomTest(const UObject* WorldContextObject, FPseudoRandomTestConfig Config);
};
