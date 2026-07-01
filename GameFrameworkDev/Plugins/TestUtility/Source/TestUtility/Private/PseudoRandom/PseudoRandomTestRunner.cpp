// Copyright xiele. All Rights Reserved.

#include "PseudoRandom/PseudoRandomTestRunner.h"
#include "PseudoRandom/PseudoRandomTestKey.h"
#include "PseudoRandom/PRBPseudoRandomSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "GameplayTags/RandomGameplayTags.h"

// =============================================================
//  测试数据生成 内部辅助
// =============================================================

/**
 * 对单个 UObject 执行 N 次 Roll，收集 Summary + Detail
 */
static void RunSingleObjectTest(
	UPRBPseudoRandomSubsystem* Subsystem,
	UObject* Object,
	const FString& InstanceName,
	float ExpectedProb,
	int32 RollCount,
	FPseudoRandomTestSummary& OutSummary,
	TArray<FPseudoRandomRollRecord>& OutRollRecords)
{
	// 设定期望概率
	Subsystem->SetExpectedProbability(Object, ExpectedProb);

	// 获取 C 值（初始概率即 C）
	const float C = Subsystem->GetCurrentProb(Object);

	OutRollRecords.Reserve(RollCount);

	int32 HitCount = 0;
	int32 HitGapCounter = 0;

	TArray<int32> FailStreakLengths;
	TArray<int32> HitStreakLengths;
	TArray<int32> HitGaps;

	for (int32 i = 0; i < RollCount; ++i)
	{
		const int32 FailCountBefore = Subsystem->GetFailCount(Object);
		const float CurrentChance = Subsystem->GetCurrentProb(Object);

		const bool bHit = Subsystem->Roll(Object);

		FPseudoRandomRollRecord Record;
		Record.InstanceName = InstanceName;
		Record.RollNumber = i + 1;
		Record.FailCountBefore = FailCountBefore;
		Record.CurrentChance = CurrentChance;
		Record.Result = bHit ? 1 : 0;
		Record.CumulativeRate = (i > 0) ? static_cast<float>(HitCount + (bHit ? 1 : 0)) / static_cast<float>(i + 1) : (bHit ? 1.0f : 0.0f);
		OutRollRecords.Add(Record);

		if (bHit)
		{
			HitCount++;
			if (HitGapCounter > 0)
			{
				HitGaps.Add(HitGapCounter);
			}
			HitGapCounter = 0;
		}
		else
		{
			HitGapCounter++;
		}
	}

	// 遍历收集中断序列
	{
		TArray<int32> FailStreaks;
		TArray<int32> HitStreaks;
		int32 fs = 0, hs = 0;
		for (const auto& Rec : OutRollRecords)
		{
			if (Rec.Result == 1)
			{
				if (fs > 0) { FailStreaks.Add(fs); fs = 0; }
				hs++;
			}
			else
			{
				if (hs > 0) { HitStreaks.Add(hs); hs = 0; }
				fs++;
			}
		}
		if (fs > 0) FailStreaks.Add(fs);
		if (hs > 0) HitStreaks.Add(hs);

		FailStreakLengths = FailStreaks;
		HitStreakLengths = HitStreaks;
	}

	// 填充 Summary
	OutSummary.InstanceName = InstanceName;
	OutSummary.ExpectedProbability = ExpectedProb;
	OutSummary.CValue = C;
	OutSummary.TotalRolls = RollCount;
	OutSummary.HitCount = HitCount;
	OutSummary.ActualHitRate = static_cast<float>(HitCount) / static_cast<float>(RollCount);
	OutSummary.Deviation = OutSummary.ActualHitRate - ExpectedProb;
	OutSummary.DeviationPercent = (ExpectedProb > 0.0f) ? (100.0f * OutSummary.Deviation / ExpectedProb) : 0.0f;

	auto SumInt32 = [](const TArray<int32>& Arr) -> int32 { int32 S = 0; for (int32 V : Arr) { S += V; } return S; };

	OutSummary.MaxFailStreak = FailStreakLengths.Num() > 0 ? FMath::Max<int32>(FailStreakLengths) : 0;
	OutSummary.AvgFailStreak = FailStreakLengths.Num() > 0
		? static_cast<float>(SumInt32(FailStreakLengths)) / static_cast<float>(FailStreakLengths.Num())
		: 0.0f;
	OutSummary.FailStreakCount = FailStreakLengths.Num();

	OutSummary.MaxHitStreak = HitStreakLengths.Num() > 0 ? FMath::Max<int32>(HitStreakLengths) : 0;
	OutSummary.AvgHitStreak = HitStreakLengths.Num() > 0
		? static_cast<float>(SumInt32(HitStreakLengths)) / static_cast<float>(HitStreakLengths.Num())
		: 0.0f;

	OutSummary.MaxHitGap = HitGaps.Num() > 0 ? FMath::Max<int32>(HitGaps) : 0;
	OutSummary.MinHitGap = HitGaps.Num() > 0 ? FMath::Min<int32>(HitGaps) : 0;
	OutSummary.AvgHitGap = HitGaps.Num() > 0
		? static_cast<float>(SumInt32(HitGaps)) / static_cast<float>(HitGaps.Num())
		: 0.0f;

	// 截断：百万次测试会产生数十万个串长，全部写入 CSV 会撑爆
	// 保留前 500 个串长，超出部分尾部注明
	auto BuildDist = [](const TArray<int32>& Streaks) -> FString {
		constexpr int32 MaxEntries = 500;
		if (Streaks.Num() <= MaxEntries)
		{
			return FString::JoinBy(Streaks, TEXT(","), [](int32 N) { return FString::FromInt(N); });
		}
		FString Result;
		for (int32 i = 0; i < MaxEntries; ++i)
		{
			if (i > 0) Result += TEXT(",");
			Result += FString::FromInt(Streaks[i]);
		}
		Result += FString::Printf(TEXT(",...(truncated,%d_total)"), Streaks.Num());
		return Result;
	};

	OutSummary.FailStreakDistribution = BuildDist(FailStreakLengths);
	OutSummary.HitStreakDistribution = BuildDist(HitStreakLengths);
}

/**
 * 对单个 Tag 执行 N 次 Roll，收集 Summary + Detail
 */
static void RunSingleTagTest(
	UPRBPseudoRandomSubsystem* Subsystem,
	FGameplayTag Tag,
	const FString& InstanceName,
	float ExpectedProb,
	int32 RollCount,
	FPseudoRandomTestSummary& OutSummary,
	TArray<FPseudoRandomRollRecord>& OutRollRecords)
{
	Subsystem->SetExpectedProbabilityForTag(Tag, ExpectedProb);

	const float C = Subsystem->GetCurrentProbForTag(Tag);

	OutRollRecords.Reserve(RollCount);

	int32 HitCount = 0;
	int32 HitGapCounter = 0;

	TArray<int32> HitGaps;

	for (int32 i = 0; i < RollCount; ++i)
	{
		const int32 FailCountBefore = Subsystem->GetFailCountForTag(Tag);
		const float CurrentChance = Subsystem->GetCurrentProbForTag(Tag);

		const bool bHit = Subsystem->RollForTag(Tag);

		FPseudoRandomRollRecord Record;
		Record.InstanceName = InstanceName;
		Record.RollNumber = i + 1;
		Record.FailCountBefore = FailCountBefore;
		Record.CurrentChance = CurrentChance;
		Record.Result = bHit ? 1 : 0;
		Record.CumulativeRate = (i > 0) ? static_cast<float>(HitCount + (bHit ? 1 : 0)) / static_cast<float>(i + 1) : (bHit ? 1.0f : 0.0f);
		OutRollRecords.Add(Record);

		if (bHit)
		{
			HitCount++;
			if (HitGapCounter > 0)
			{
				HitGaps.Add(HitGapCounter);
			}
			HitGapCounter = 0;
		}
		else
		{
			HitGapCounter++;
		}
	}

	// 遍历收集中断序列
	TArray<int32> FailStreaks;
	TArray<int32> HitStreaks;
	int32 fs = 0, hs = 0;
	for (const auto& Rec : OutRollRecords)
	{
		if (Rec.Result == 1)
		{
			if (fs > 0) { FailStreaks.Add(fs); fs = 0; }
			hs++;
		}
		else
		{
			if (hs > 0) { HitStreaks.Add(hs); hs = 0; }
			fs++;
		}
	}
	if (fs > 0) FailStreaks.Add(fs);
	if (hs > 0) HitStreaks.Add(hs);

	OutSummary.InstanceName = InstanceName;
	OutSummary.ExpectedProbability = ExpectedProb;
	OutSummary.CValue = C;
	OutSummary.TotalRolls = RollCount;
	OutSummary.HitCount = HitCount;
	OutSummary.ActualHitRate = static_cast<float>(HitCount) / static_cast<float>(RollCount);
	OutSummary.Deviation = OutSummary.ActualHitRate - ExpectedProb;
	OutSummary.DeviationPercent = (ExpectedProb > 0.0f) ? (100.0f * OutSummary.Deviation / ExpectedProb) : 0.0f;

	auto SumInt32 = [](const TArray<int32>& Arr) -> int32 { int32 S = 0; for (int32 V : Arr) { S += V; } return S; };

	OutSummary.MaxFailStreak = FailStreaks.Num() > 0 ? FMath::Max<int32>(FailStreaks) : 0;
	OutSummary.AvgFailStreak = FailStreaks.Num() > 0
		? static_cast<float>(SumInt32(FailStreaks)) / static_cast<float>(FailStreaks.Num())
		: 0.0f;
	OutSummary.FailStreakCount = FailStreaks.Num();

	OutSummary.MaxHitStreak = HitStreaks.Num() > 0 ? FMath::Max<int32>(HitStreaks) : 0;
	OutSummary.AvgHitStreak = HitStreaks.Num() > 0
		? static_cast<float>(SumInt32(HitStreaks)) / static_cast<float>(HitStreaks.Num())
		: 0.0f;

	OutSummary.MaxHitGap = HitGaps.Num() > 0 ? FMath::Max<int32>(HitGaps) : 0;
	OutSummary.MinHitGap = HitGaps.Num() > 0 ? FMath::Min<int32>(HitGaps) : 0;
	OutSummary.AvgHitGap = HitGaps.Num() > 0
		? static_cast<float>(SumInt32(HitGaps)) / static_cast<float>(HitGaps.Num())
		: 0.0f;

	// 截断保护（与 Object Key 测试逻辑一致）
	auto BuildDist = [](const TArray<int32>& Streaks) -> FString {
		constexpr int32 MaxEntries = 500;
		if (Streaks.Num() <= MaxEntries)
		{
			return FString::JoinBy(Streaks, TEXT(","), [](int32 N) { return FString::FromInt(N); });
		}
		FString Result;
		for (int32 i = 0; i < MaxEntries; ++i)
		{
			if (i > 0) Result += TEXT(",");
			Result += FString::FromInt(Streaks[i]);
		}
		Result += FString::Printf(TEXT(",...(truncated,%d_total)"), Streaks.Num());
		return Result;
	};

	OutSummary.FailStreakDistribution = BuildDist(FailStreaks);
	OutSummary.HitStreakDistribution = BuildDist(HitStreaks);
}

/** 写 Summary CSV */
static FString WriteSummaryCSV(const TArray<FPseudoRandomTestSummary>& Summaries)
{
	FString CSV;
	CSV += TEXT("InstanceName,ExpectedProbability,CValue,TotalRolls,HitCount,ActualHitRate,Deviation,DeviationPercent,");
	CSV += TEXT("MaxFailStreak,AvgFailStreak,FailStreakCount,");
	CSV += TEXT("MaxHitStreak,AvgHitStreak,");
	CSV += TEXT("MaxHitGap,MinHitGap,AvgHitGap,");
	CSV += TEXT("FailStreakDistribution,HitStreakDistribution\n");

	for (const FPseudoRandomTestSummary& S : Summaries)
	{
		CSV += FString::Printf(TEXT("%s,%.6f,%.6f,%d,%d,%.6f,%.6f,%.2f,"),
			*S.InstanceName, S.ExpectedProbability, S.CValue,
			S.TotalRolls, S.HitCount, S.ActualHitRate, S.Deviation, S.DeviationPercent);
		CSV += FString::Printf(TEXT("%d,%.2f,%d,"),
			S.MaxFailStreak, S.AvgFailStreak, S.FailStreakCount);
		CSV += FString::Printf(TEXT("%d,%.2f,"),
			S.MaxHitStreak, S.AvgHitStreak);
		CSV += FString::Printf(TEXT("%d,%d,%.2f,"),
			S.MaxHitGap, S.MinHitGap, S.AvgHitGap);
		CSV += FString::Printf(TEXT("\"%s\",\"%s\"\n"),
			*S.FailStreakDistribution, *S.HitStreakDistribution);
	}

	return CSV;
}

/** 写 Detail CSV 纯数据行（不含表头，用于流式追加） */
static FString WriteDetailRows(const TArray<FPseudoRandomRollRecord>& Records)
{
	FString CSV;
	for (const FPseudoRandomRollRecord& R : Records)
	{
		CSV += FString::Printf(TEXT("%s,%d,%d,%.6f,%d,%.6f\n"),
			*R.InstanceName, R.RollNumber, R.FailCountBefore,
			R.CurrentChance, R.Result, R.CumulativeRate);
	}
	return CSV;
}

/** 写 Detail CSV（含表头） */
static FString WriteDetailCSV(const TArray<FPseudoRandomRollRecord>& Records)
{
	FString CSV;
	CSV += TEXT("InstanceName,RollNumber,FailCountBefore,CurrentChance,Result,CumulativeRate\n");
	CSV += WriteDetailRows(Records);
	return CSV;
}

// =============================================================
//  Object Key 测试（蓝图入口）
// =============================================================

void UPseudoRandomTestRunner::RunObjectPseudoRandomTest(const UObject* WorldContextObject, FPseudoRandomTestConfig Config)
{
	if (Config.ProbabilityLevels.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunObjectPseudoRandomTest] ProbabilityLevels is empty"));
		return;
	}

	// 获取 PRB 伪随机子系统
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunObjectPseudoRandomTest] Failed to get World from context"));
		return;
	}
	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunObjectPseudoRandomTest] Failed to get GameInstance"));
		return;
	}
	UPRBPseudoRandomSubsystem* Subsystem = GameInstance->GetSubsystem<UPRBPseudoRandomSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunObjectPseudoRandomTest] Failed to get UPRBPseudoRandomSubsystem"));
		return;
	}

	// 输出目录
	const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("PseudoRandomTests");
	IFileManager::Get().MakeDirectory(*SaveDir, true);

	TArray<FPseudoRandomTestSummary> AllSummaries;

	const int32 TotalInstances = Config.ProbabilityLevels.Num() * Config.ItemsPerProbability;
	UE_LOG(LogTemp, Log, TEXT("[RunObjectPseudoRandomTest] 开始测试: %d 概率等级 × %d 实例 × %d 判定 = 共 %d 实例"),
		Config.ProbabilityLevels.Num(), Config.ItemsPerProbability, Config.RollsPerItem, TotalInstances);
	UE_LOG(LogTemp, Log, TEXT("[RunObjectPseudoRandomTest] 输出目录: %s"), *SaveDir);

	// 复用单个 TestObject，避免大量 NewObject 堆积
	UPseudoRandomTestKey* TestObject = NewObject<UPseudoRandomTestKey>();
	TestObject->AddToRoot();  // 防止 GC

	// Detail CSV 流式写入（不积累在内存）
	const FString DetailPath = SaveDir / TEXT("PseudoRandomTest_Object_Detail.csv");
	FArchive* DetailWriter = nullptr;
	if (Config.bExportDetailCSV)
	{
		// 写 CSV 头
		FFileHelper::SaveStringToFile(TEXT("InstanceName,RollNumber,FailCountBefore,CurrentChance,Result,CumulativeRate\n"),
			*DetailPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		DetailWriter = IFileManager::Get().CreateFileWriter(*DetailPath, EFileWrite::FILEWRITE_Append);
	}

	int32 CompletedInstances = 0;
	int32 TotalDetailRecords = 0;

	for (float Prob : Config.ProbabilityLevels)
	{
		const FString ProbLabel = FString::Printf(TEXT("%.2f"), Prob * 100.0f);

		for (int32 ItemIdx = 0; ItemIdx < Config.ItemsPerProbability; ++ItemIdx)
		{
			const FString InstanceName = FString::Printf(TEXT("Object_Prob_%s_Item_%02d"), *ProbLabel, ItemIdx + 1);

			FPseudoRandomTestSummary Summary;
			TArray<FPseudoRandomRollRecord> Records;

			RunSingleObjectTest(Subsystem, TestObject, InstanceName, Prob, Config.RollsPerItem, Summary, Records);

			AllSummaries.Add(Summary);

			// 流式追加 Detail CSV（不含表头，表头已在文件开头写入）
			if (DetailWriter)
			{
				const FString DetailBlock = WriteDetailRows(Records);
				FTCHARToUTF8 DetailUTF8(*DetailBlock);
				DetailWriter->Serialize(const_cast<char*>(reinterpret_cast<const char*>(DetailUTF8.Get())), DetailUTF8.Length());
				TotalDetailRecords += Records.Num();
			}

			// 重置 Object 状态：从 PRB 移除后即可复用
			Subsystem->RemoveObject(TestObject);

			CompletedInstances++;

			// 每 50 个实例触发一次 GC，防止内存持续增长
			if (CompletedInstances % 50 == 0)
			{
				GEngine->ForceGarbageCollection(true);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("  进度: %d/%d 实例完成"), CompletedInstances, TotalInstances);
	}

	// 清理复用对象
	TestObject->RemoveFromRoot();
	TestObject->MarkAsGarbage();

	if (DetailWriter)
	{
		DetailWriter->Flush();
		DetailWriter->Close();
		delete DetailWriter;
	}

	// 写入 Summary CSV
	const FString SummaryPath = SaveDir / TEXT("PseudoRandomTest_Object_Summary.csv");
	FFileHelper::SaveStringToFile(WriteSummaryCSV(AllSummaries), *SummaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Log, TEXT("[RunObjectPseudoRandomTest] Summary CSV 已写入: %s (%d 行)"), *SummaryPath, AllSummaries.Num());

	if (Config.bExportDetailCSV)
	{
		UE_LOG(LogTemp, Log, TEXT("[RunObjectPseudoRandomTest] Detail CSV 已写入: %s (%d 行)"), *DetailPath, TotalDetailRecords);
	}

	UE_LOG(LogTemp, Log, TEXT("[RunObjectPseudoRandomTest] 测试完成！共 %d 实例，%d 条详细记录"), AllSummaries.Num(), TotalDetailRecords);
}

// =============================================================
//  Tag Key 测试（蓝图入口）
// =============================================================

void UPseudoRandomTestRunner::RunTagPseudoRandomTest(const UObject* WorldContextObject, FPseudoRandomTestConfig Config)
{
	if (Config.ProbabilityLevels.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunTagPseudoRandomTest] ProbabilityLevels is empty"));
		return;
	}

	// 获取 PRB 伪随机子系统
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunTagPseudoRandomTest] Failed to get World from context"));
		return;
	}
	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunTagPseudoRandomTest] Failed to get GameInstance"));
		return;
	}
	UPRBPseudoRandomSubsystem* Subsystem = GameInstance->GetSubsystem<UPRBPseudoRandomSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunTagPseudoRandomTest] Failed to get UPRBPseudoRandomSubsystem"));
		return;
	}

	const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("PseudoRandomTests");
	IFileManager::Get().MakeDirectory(*SaveDir, true);

	TArray<FPseudoRandomTestSummary> AllSummaries;

	const int32 TotalInstances = Config.ProbabilityLevels.Num() * Config.ItemsPerProbability;
	UE_LOG(LogTemp, Log, TEXT("[RunTagPseudoRandomTest] 开始测试: %d 概率等级 × %d 实例 × %d 判定 = 共 %d 实例"),
		Config.ProbabilityLevels.Num(), Config.ItemsPerProbability, Config.RollsPerItem, TotalInstances);
	UE_LOG(LogTemp, Log, TEXT("[RunTagPseudoRandomTest] 输出目录: %s"), *SaveDir);

	// Detail CSV 流式写入
	const FString DetailPath = SaveDir / TEXT("PseudoRandomTest_Tag_Detail.csv");
	FArchive* DetailWriter = nullptr;
	if (Config.bExportDetailCSV)
	{
		FFileHelper::SaveStringToFile(TEXT("InstanceName,RollNumber,FailCountBefore,CurrentChance,Result,CumulativeRate\n"),
			*DetailPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		DetailWriter = IFileManager::Get().CreateFileWriter(*DetailPath, EFileWrite::FILEWRITE_Append);
	}

	int32 CompletedInstances = 0;
	int32 TotalDetailRecords = 0;

	for (float Prob : Config.ProbabilityLevels)
	{
		const FString ProbLabel = FString::Printf(TEXT("%.2f"), Prob * 100.0f);

		for (int32 ItemIdx = 0; ItemIdx < Config.ItemsPerProbability; ++ItemIdx)
		{
			// 所有测试实例复用同一个预注册的 GameplayTag（实例间通过 RemoveTag 隔离状态）
			const FGameplayTag Tag = CommonGameplayTags::TAG_RANDOM_PSEUDO_TEST;
			const FString InstanceName = FString::Printf(TEXT("Tag_Prob_%s_Item_%02d"), *ProbLabel, ItemIdx + 1);

			FPseudoRandomTestSummary Summary;
			TArray<FPseudoRandomRollRecord> Records;

			RunSingleTagTest(Subsystem, Tag, InstanceName, Prob, Config.RollsPerItem, Summary, Records);

			AllSummaries.Add(Summary);

			// 流式追加 Detail CSV
			if (DetailWriter)
			{
				const FString DetailBlock = WriteDetailRows(Records);
				FTCHARToUTF8 DetailUTF8(*DetailBlock);
				DetailWriter->Serialize(const_cast<char*>(reinterpret_cast<const char*>(DetailUTF8.Get())), DetailUTF8.Length());
				TotalDetailRecords += Records.Num();
			}

			// 测试完毕，清理 Tag 状态
			Subsystem->RemoveTag(Tag);

			CompletedInstances++;

			// 每 50 个实例触发一次 GC
			if (CompletedInstances % 50 == 0)
			{
				GEngine->ForceGarbageCollection(true);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("  进度: %d/%d 实例完成"), CompletedInstances, TotalInstances);
	}

	if (DetailWriter)
	{
		DetailWriter->Flush();
		DetailWriter->Close();
		delete DetailWriter;
	}

	// 写入 Summary CSV
	const FString SummaryPath = SaveDir / TEXT("PseudoRandomTest_Tag_Summary.csv");
	FFileHelper::SaveStringToFile(WriteSummaryCSV(AllSummaries), *SummaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Log, TEXT("[RunTagPseudoRandomTest] Summary CSV 已写入: %s (%d 行)"), *SummaryPath, AllSummaries.Num());

	if (Config.bExportDetailCSV)
	{
		UE_LOG(LogTemp, Log, TEXT("[RunTagPseudoRandomTest] Detail CSV 已写入: %s (%d 行)"), *DetailPath, TotalDetailRecords);
	}

	UE_LOG(LogTemp, Log, TEXT("[RunTagPseudoRandomTest] 测试完成！共 %d 实例，%d 条详细记录"), AllSummaries.Num(), TotalDetailRecords);
}
