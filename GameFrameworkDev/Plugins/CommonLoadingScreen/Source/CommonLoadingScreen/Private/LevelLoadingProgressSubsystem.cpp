// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelLoadingProgressSubsystem.h"
#include "CommonLoadingScreenLog.h"
#include "CommonLoadingScreenSettings.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelLoadingProgressSubsystem)

//////////////////////////////////////////////////////////////////////
// ULevelLoadingProgressSubsystem
//////////////////////////////////////////////////////////////////////

void ULevelLoadingProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 从项目设置中读取配置
	MinimumLoadingScreenDisplayTimeSecs = GetDefault<UCommonLoadingScreenSettings>()->MinimumLoadingScreenDisplayTime;

	// 订阅 PreLoadMap（带 Context，可区分 GameInstance）
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ULevelLoadingProgressSubsystem::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULevelLoadingProgressSubsystem::HandlePostLoadMap);

	UE_LOG(LogLevelLoadingProgress, Log, TEXT("LevelLoadingProgressSubsystem 已初始化"));
}

void ULevelLoadingProgressSubsystem::Deinitialize()
{
	// 停止进度计算 Ticker
	if (ProgressTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ProgressTickerHandle);
		ProgressTickerHandle.Reset();
	}
	
	// 取消地图加载委托
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	UE_LOG(LogLevelLoadingProgress, Log, TEXT("LevelLoadingProgressSubsystem 已销毁"));

	Super::Deinitialize();
}

bool ULevelLoadingProgressSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 仅客户端需要加载进度追踪
	const UGameInstance* GameInstance = CastChecked<UGameInstance>(Outer);
	const bool bIsServerWorld = GameInstance->IsDedicatedServerInstance();
	return !bIsServerWorld;
}

// ================================================================
// Public API
// ================================================================

float ULevelLoadingProgressSubsystem::GetPreciseLoadingProgress() const
{
	return CachedProgress;
}

ELevelLoadingPhase ULevelLoadingProgressSubsystem::GetCurrentLoadingPhase() const
{
	return CurrentLoadingPhase;
}

float ULevelLoadingProgressSubsystem::GetRawAsyncLoadPercentage() const
{
	return GetAsyncLoadPercentage(NAME_None);
}

bool ULevelLoadingProgressSubsystem::IsLoadingInProgress() const
{
	return bCurrentlyInLoadMap &&
		CurrentLoadingPhase != ELevelLoadingPhase::None &&
		CurrentLoadingPhase != ELevelLoadingPhase::Completed;
}

// ================================================================
// Phase Management
// ================================================================

void ULevelLoadingProgressSubsystem::SetLoadingPhase(ELevelLoadingPhase NewPhase)
{
	if (CurrentLoadingPhase == NewPhase)
	{
		return;
	}

	const ELevelLoadingPhase OldPhase = CurrentLoadingPhase;
	CurrentLoadingPhase = NewPhase;
	PhaseStartTime = FPlatformTime::Seconds();

	// 进入 WorldInit 时暂停世界，进入 Completed 时恢复
	if (UWorld* World = GetWorld())
	{
		if (NewPhase == ELevelLoadingPhase::WorldInit)
		{
			UGameplayStatics::SetGamePaused(World, true);
		}
		else if (NewPhase == ELevelLoadingPhase::Completed)
		{
			UGameplayStatics::SetGamePaused(World, false);
		}
	}

	UE_LOG(LogLevelLoadingProgress, Log, TEXT("[LoadingProgress] 阶段切换: %s -> %s"),
		*UEnum::GetValueAsString(OldPhase), *UEnum::GetValueAsString(NewPhase));
}

void ULevelLoadingProgressSubsystem::TickProgress()
{
	const double Elapsed = FPlatformTime::Seconds() - PhaseStartTime;

	// 阶段自动推进
	switch (CurrentLoadingPhase)
	{
	case ELevelLoadingPhase::Preparing:
	{
		// Preparing → AsyncLoading：引擎开始异步加载时自动切换
		if (GetAsyncLoadPercentage(NAME_None) >= 0.0f)
		{
			SetLoadingPhase(ELevelLoadingPhase::AsyncLoading);
		}
		break;
	}
	case ELevelLoadingPhase::AsyncLoading:
	{
		// 由 CalculatePhaseProgress 通过引擎真实进度驱动
		// 阶段切换由 HandlePostLoadMap 负责
		break;
	}
	case ELevelLoadingPhase::WorldInit:
	{
		// WorldInit → Finalizing：超过最小显示时长后自动收尾
		if (Elapsed >= MinimumLoadingScreenDisplayTimeSecs)
		{
			SetLoadingPhase(ELevelLoadingPhase::Finalizing);
		}
		break;
	}
	case ELevelLoadingPhase::Finalizing:
	{
		// Finalizing → Completed：短暂收尾后标记完成
		if (Elapsed >= 0.3)
		{
			SetLoadingPhase(ELevelLoadingPhase::Completed);
			bCurrentlyInLoadMap = false;
		}
		break;
	}
	case ELevelLoadingPhase::Completed:
	{
		CachedProgress = 100.0f;

		// 完成后移除 Ticker
		if (ProgressTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(ProgressTickerHandle);
			ProgressTickerHandle.Reset();
		}
		return;
	}
	default:
		break;
	}

	// 计算并缓存当前进度
	CachedProgress = CalculatePhaseProgress();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, ProgressTickInterval, FColor::Cyan,
			FString::Printf(TEXT("[LoadingProgress] Phase: %s | %.1f%%"),
				*UEnum::GetValueAsString(CurrentLoadingPhase), CachedProgress));
	}
}

float ULevelLoadingProgressSubsystem::CalculatePhaseProgress() const
{
	const double Elapsed = FPlatformTime::Seconds() - PhaseStartTime;

	switch (CurrentLoadingPhase)
	{
	case ELevelLoadingPhase::Preparing:
	{
		// 准备阶段极短，用时间基线估算 0 -> PreparingWeight
		const float Ratio = FMath::Clamp(static_cast<float>(Elapsed / 0.5), 0.0f, 1.0f);
		return Ratio * PreparingWeight;
	}

	case ELevelLoadingPhase::AsyncLoading:
	{
		// 核心阶段：使用引擎 GetAsyncLoadPercentage 真实进度
		const float Raw = GetAsyncLoadPercentage(NAME_None);
		if (Raw < 0.0f)
		{
			return PreparingWeight + AsyncLoadWeight;
		}
		return PreparingWeight + (Raw / 100.0f) * AsyncLoadWeight;
	}

	case ELevelLoadingPhase::WorldInit:
	{
		// 引擎无进度回调，用时间平滑估算
		// EaseOut 曲线 (1 - (1-t)^2)：前快后慢，保证进度始终增长
		const float Ratio = FMath::Clamp(static_cast<float>(Elapsed / MinimumLoadingScreenDisplayTimeSecs), 0.0f, 1.0f);
		const float Smoothed = 1.0f - FMath::Square(1.0f - Ratio);
		return PreparingWeight + AsyncLoadWeight + Smoothed * WorldInitWeight;
	}

	case ELevelLoadingPhase::Finalizing:
	{
		// 收尾阶段：平滑接近 100%
		const float Ratio = FMath::Clamp(static_cast<float>(Elapsed / 0.3), 0.0f, 1.0f);
		return PreparingWeight + AsyncLoadWeight + WorldInitWeight + Ratio * FinalizeWeight;
	}

	default:
		return 100.0f;
	}
}

// ================================================================
// Map Loading Callbacks
// ================================================================

void ULevelLoadingProgressSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		return;
	}

	UE_LOG(LogLevelLoadingProgress, Log, TEXT("HandlePreLoadMap: MapName=[%s]"), *MapName);

	bCurrentlyInLoadMap = true;
	PreLoadMapName = MapName;
	LoadingStartTime = FPlatformTime::Seconds();

	// 进入准备阶段
	SetLoadingPhase(ELevelLoadingPhase::Preparing);

	// 启动 Ticker 驱动内部进度计算（不广播）
	if (ProgressTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ProgressTickerHandle);
	}
	FTickerDelegate ProgressDelegate;
	ProgressDelegate.BindLambda([this](float) -> bool
	{
		TickProgress();
		return true;
	});
	ProgressTickerHandle = FTSTicker::GetCoreTicker().AddTicker(ProgressDelegate, ProgressTickInterval);
}

void ULevelLoadingProgressSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld == nullptr || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	UE_LOG(LogLevelLoadingProgress, Log, TEXT("HandlePostLoadMap: World=[%s]"), *GetNameSafe(LoadedWorld));

	// 进入世界初始化阶段
	SetLoadingPhase(ELevelLoadingPhase::WorldInit);

	UE_LOG(LogLevelLoadingProgress, Log, TEXT("HandlePostLoadMap 完成"));
}
