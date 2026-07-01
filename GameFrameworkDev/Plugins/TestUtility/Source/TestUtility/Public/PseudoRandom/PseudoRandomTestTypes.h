// Copyright xiele. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PseudoRandomTestTypes.generated.h"

/**
 * 单次 Roll 的详细记录
 * 用于 Detail CSV，每一行对应一次判定
 */
USTRUCT(BlueprintType)
struct TESTUTILITY_API FPseudoRandomRollRecord
{
	GENERATED_BODY()

	/** 所属测试实例名称 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FString InstanceName;

	/** Roll 序号（1-based） */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	int32 RollNumber = 0;

	/** 本次判定前的累积失败次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	int32 FailCountBefore = 0;

	/** 本次判定的实际概率 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float CurrentChance = 0.0f;

	/** 判定结果：1=命中, 0=未命中 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	int32 Result = 0;

	/** 截止本次的累积命中率 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float CumulativeRate = 0.0f;
};

/**
 * 单个实例测试汇总
 * 每个概率等级 × 每个实例产生一行汇总
 */
USTRUCT(BlueprintType)
struct TESTUTILITY_API FPseudoRandomTestSummary
{
	GENERATED_BODY()

	/** 实例名称 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FString InstanceName;

	/** 期望概率（设定值） */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float ExpectedProbability = 0.0f;

	/** PRB 伪随机 C 值 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float CValue = 0.0f;

	/** 总判定次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	int32 TotalRolls = 0;

	/** 命中次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	int32 HitCount = 0;

	/** 实际命中率 = HitCount / TotalRolls */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float ActualHitRate = 0.0f;

	/** 绝对偏差 = ActualHitRate - ExpectedProbability */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float Deviation = 0.0f;

	/** 相对偏差(%) = 100 * Deviation / ExpectedProbability */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float DeviationPercent = 0.0f;

	/** 最长连续失败次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	int32 MaxFailStreak = 0;

	/** 平均连续失败次数（所有失败串的平均长度） */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float AvgFailStreak = 0.0f;

	/** 失败串数量 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	int32 FailStreakCount = 0;

	/** 最长连续命中次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	int32 MaxHitStreak = 0;

	/** 平均连续命中次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float AvgHitStreak = 0.0f;

	/** 两次命中之间的最大判定间隔 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	int32 MaxHitGap = 0;

	/** 两次命中之间的最小判定间隔（不含连中的间距0） */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	int32 MinHitGap = 0;

	/** 两次命中之间的平均判定间隔（失败后首次命中视为间隔结束） */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float AvgHitGap = 0.0f;

	/** 每次失败串长度分布（逗号分隔），如 "1,2,3,1,4,2" 表示发生 6 次失败串 */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FString FailStreakDistribution;

	/** 每次命中串长度分布（逗号分隔） */
	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FString HitStreakDistribution;
};

/**
 * 测试批次配置
 */
USTRUCT(BlueprintType)
struct TESTUTILITY_API FPseudoRandomTestConfig
{
	GENERATED_BODY()

	/** 每概率等级创建的实例数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	int32 ItemsPerProbability = 100;

	/** 每实例执行判定次数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	int32 RollsPerItem = 100000;

	/** 测试用概率列表（1%~99% 全覆盖） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	TArray<float> ProbabilityLevels = {
		0.01f, 0.02f, 0.03f, 0.04f, 0.05f, 0.06f, 0.07f, 0.08f, 0.09f, 0.10f,
		0.11f, 0.12f, 0.13f, 0.14f, 0.15f, 0.16f, 0.17f, 0.18f, 0.19f, 0.20f,
		0.21f, 0.22f, 0.23f, 0.24f, 0.25f, 0.26f, 0.27f, 0.28f, 0.29f, 0.30f,
		0.31f, 0.32f, 0.33f, 0.34f, 0.35f, 0.36f, 0.37f, 0.38f, 0.39f, 0.40f,
		0.41f, 0.42f, 0.43f, 0.44f, 0.45f, 0.46f, 0.47f, 0.48f, 0.49f, 0.50f,
		0.51f, 0.52f, 0.53f, 0.54f, 0.55f, 0.56f, 0.57f, 0.58f, 0.59f, 0.60f,
		0.61f, 0.62f, 0.63f, 0.64f, 0.65f, 0.66f, 0.67f, 0.68f, 0.69f, 0.70f,
		0.71f, 0.72f, 0.73f, 0.74f, 0.75f, 0.76f, 0.77f, 0.78f, 0.79f, 0.80f,
		0.81f, 0.82f, 0.83f, 0.84f, 0.85f, 0.86f, 0.87f, 0.88f, 0.89f, 0.90f,
		0.91f, 0.92f, 0.93f, 0.94f, 0.95f, 0.96f, 0.97f, 0.98f, 0.99f
	};

	/** 是否同时输出 Detail CSV（每 Roll 一行） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
	bool bExportDetailCSV = true;
};
