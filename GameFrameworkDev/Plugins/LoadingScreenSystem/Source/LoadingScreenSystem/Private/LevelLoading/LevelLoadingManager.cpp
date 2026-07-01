// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelLoading/LevelLoadingManager.h"
#include "LoadingScreenSettings.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelLoadingManager)

DEFINE_LOG_CATEGORY(LogLevelLoading);

////////////////////////////////////////////////////////////////////
// ULevelLoadingManager
////////////////////////////////////////////////////////////////////

void ULevelLoadingManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 从项目设置中读取配置
	MinimumLevelLoadingScreenDisplayTimeSecs = GetDefault<ULoadingScreenSettings>()->MinimumLevelLoadingScreenDisplayTime;

	// 订阅 PreLoadMap（带 Context，可区分 GameInstance）
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ULevelLoadingManager::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULevelLoadingManager::HandlePostLoadMap);

	UE_LOG(LogLevelLoading, Log, TEXT("LevelLoadingManager 已初始化"));
}

void ULevelLoadingManager::Deinitialize()
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

	UE_LOG(LogLevelLoading, Log, TEXT("LevelLoadingManager 已销毁"));

	Super::Deinitialize();
}

bool ULevelLoadingManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// 仅客户端需要加载进度追踪
	const UGameInstance* GameInstance = CastChecked<UGameInstance>(Outer);
	const bool bIsServerWorld = GameInstance->IsDedicatedServerInstance();
	return !bIsServerWorld;
}

// ================================================================
// Public API
// ================================================================

float ULevelLoadingManager::GetPreciseLoadingProgress() const
{
	return CachedProgress;
}

ELevelLoadingPhase ULevelLoadingManager::GetCurrentLoadingPhase() const
{
	return CurrentLoadingPhase;
}

float ULevelLoadingManager::GetRawAsyncLoadPercentage() const
{
	return GetAsyncLoadPercentage(NAME_None);
}

bool ULevelLoadingManager::IsLoadingInProgress() const
{
	return bCurrentlyInLoadMap &&
		CurrentLoadingPhase != ELevelLoadingPhase::None &&
		CurrentLoadingPhase != ELevelLoadingPhase::Completed;
}

void ULevelLoadingManager::PrepareCloseLoadingScreen()
{
	
}

// ================================================================
// Phase Management
// ================================================================

void ULevelLoadingManager::SetLoadingPhase(ELevelLoadingPhase NewPhase)
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

	UE_LOG(LogLevelLoading, Log, TEXT("[LoadingProgress] 阶段切换: %s -> %s"),
		*UEnum::GetValueAsString(OldPhase), *UEnum::GetValueAsString(NewPhase));
}

void ULevelLoadingManager::TickProgress()
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
		if (Elapsed >= MinimumLevelLoadingScreenDisplayTimeSecs)
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

float ULevelLoadingManager::CalculatePhaseProgress() const
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
		const float Ratio = FMath::Clamp(static_cast<float>(Elapsed / MinimumLevelLoadingScreenDisplayTimeSecs), 0.0f, 1.0f);
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

void ULevelLoadingManager::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		return;
	}

	UE_LOG(LogLevelLoading, Log, TEXT("HandlePreLoadMap: MapName=[%s]"), *MapName);

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

void ULevelLoadingManager::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld == nullptr || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	UE_LOG(LogLevelLoading, Log, TEXT("HandlePostLoadMap: World=[%s]"), *GetNameSafe(LoadedWorld));

	// 进入世界初始化阶段
	SetLoadingPhase(ELevelLoadingPhase::WorldInit);

	UE_LOG(LogLevelLoading, Log, TEXT("HandlePostLoadMap 完成"));
}
