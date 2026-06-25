// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSOCacheManager.h"
#include "PSOCacheMonitorWidget.h"
#include "PSOCacheSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RHI.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "ShaderPipelineCache.h"
#include "UObject/UObjectGlobals.h"

// ---- 控制台命令 ----

/** 查找当前 Game/PIE World */
static UWorld* GetCurrentPlayWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
		{
			return Context.World();
		}
	}
	return nullptr;
}

static FAutoConsoleCommand CmdPSOCoverageStart(
	TEXT("PSOCoverage.Start"),
	TEXT("Start the automated PSO coverage strategy (Material + Niagara)"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (UWorld* World = GetCurrentPlayWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UPSOCacheManager* Mgr = GI->GetSubsystem<UPSOCacheManager>())
				{
					Mgr->BeginAutomatedCoverageStrategy();
				}
			}
		}
	})
);

static FAutoConsoleCommand CmdPSOCoverageStop(
	TEXT("PSOCoverage.Stop"),
	TEXT("Stop the automated PSO coverage strategy"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (UWorld* World = GetCurrentPlayWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UPSOCacheManager* Mgr = GI->GetSubsystem<UPSOCacheManager>())
				{
					Mgr->EndAutomatedCoverageStrategy();
				}
			}
		}
	})
);

void UPSOCacheManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 解析启动命令行参数
	bAutoStartCoverage = FParse::Param(FCommandLine::Get(), TEXT("psosysautocoverage"));
	bAutoQuitGame     = FParse::Param(FCommandLine::Get(), TEXT("psosysautoquitgame"));

	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::Initialize - PSO Cache Manager Initialized (AutoCoverage=%d, AutoQuit=%d)"),
		bAutoStartCoverage, bAutoQuitGame);

	if (bAutoStartCoverage)
	{
		HandleAutoStartCoverage();
	}
}

void UPSOCacheManager::HandleAutoStartCoverage()
{
	// 延迟到 World 就绪后：先切入 Warmup 地图，再启动覆盖策略
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			UWorld* World = GetWorld();
			if (!World || !GEngine || !GEngine->GameViewport)
			{
				return true; // 继续等待
			}

			// 检查是否需要切换到 Warmup 地图
			const UPSOCacheSettings* Settings = GetDefault<UPSOCacheSettings>();
			if (Settings && Settings->PSOCoverageMap.IsValid())
			{
				const FString CurrentMap = World->GetMapName();
				const FString TargetMap  = FPaths::GetBaseFilename(Settings->PSOCoverageMap.GetAssetPathString());
				if (!CurrentMap.Equals(TargetMap, ESearchCase::IgnoreCase))
				{
					UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::HandleAutoStartCoverage - Switching to warmup map: %s (current: %s)"), *TargetMap, *CurrentMap);

					// 切换关卡前，移除 GameViewport 上所有 UI Widget
					GEngine->GameViewport->RemoveAllViewportWidgets();

					UGameplayStatics::OpenLevel(World, FName(*TargetMap));
					return false; // OpenLevel 触发新 World，新 World 的 Initialize 会重新触发
				}
			}

			UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::HandleAutoStartCoverage - World ready, auto-starting coverage strategy"));
			BeginAutomatedCoverageStrategy();
			return false; // 只执行一次
		}),
		0.0f
	);
}

void UPSOCacheManager::Deinitialize()
{
	// 确保 Tick 已注销
	if (bCoverageActive)
	{
		EndAutomatedCoverageStrategy();
	}

	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::Deinitialize - PSO Cache Manager Shutdown"));

	Super::Deinitialize();
}

bool UPSOCacheManager::IsPSOCacheReady() const
{
	return bPSOCacheReady;
}

int32 UPSOCacheManager::GetCachedPSOCount() const
{
	return CachedPSOCount;
}

int32 UPSOCacheManager::GetPrecompilesRemaining() const
{
	return FShaderPipelineCache::NumPrecompilesRemaining();
}

void UPSOCacheManager::LoadBuiltInPSOCache()
{
	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::LoadBuiltInPSOCache - Loading built-in PSO cache..."));

	// TODO: 实现内建 PSO 缓存加载逻辑

	bPSOCacheReady = true;
}

TArray<TSoftObjectPtr<UMaterial>> UPSOCacheManager::GetAllMaterials()
{
	TArray<TSoftObjectPtr<UMaterial>> Materials;

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(UMaterial::StaticClass()->GetClassPathName(), AssetDataList, /*bSearchSubClasses=*/true);

	for (const FAssetData& AssetData : AssetDataList)
	{
		Materials.Add(TSoftObjectPtr<UMaterial>(AssetData.GetSoftObjectPath()));
	}

	return Materials;
}

TArray<TSoftObjectPtr<UNiagaraSystem>> UPSOCacheManager::GetAllNiagaraParticleSystems()
{
	TArray<TSoftObjectPtr<UNiagaraSystem>> NiagaraSystems;

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(UNiagaraSystem::StaticClass()->GetClassPathName(), AssetDataList, /*bSearchSubClasses=*/true);

	for (const FAssetData& AssetData : AssetDataList)
	{
		NiagaraSystems.Add(TSoftObjectPtr<UNiagaraSystem>(AssetData.GetSoftObjectPath()));
	}

	return NiagaraSystems;
}

void UPSOCacheManager::ClearPSOCache()
{
	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::ClearPSOCache - Clearing PSO cache..."));

	CachedPSOCount = 0;
	bPSOCacheReady = false;
}

// ---- 自动化覆盖策略 ----

void UPSOCacheManager::BeginAutomatedCoverageStrategy()
{
	if (bCoverageActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("UPSOCacheManager::BeginAutomatedCoverageStrategy - Already running, ignored"));
		return;
	}

	const UPSOCacheSettings* Settings = GetDefault<UPSOCacheSettings>();
	if (!Settings)
	{
		UE_LOG(LogTemp, Warning, TEXT("UPSOCacheManager::BeginAutomatedCoverageStrategy - Settings not found"));
		return;
	}

	bCoverageActive = true;
	CoveragePhase = EPSOCoveragePhase::Material;
	CoverageElapsedTotal = 0.0f;
	CoverageCompleteElapsed = 0.0f;

	// 依次判断并开启材质覆盖
	if (Settings->bEnableMaterialCoverage)
	{
		UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::BeginAutomatedCoverageStrategy - Material Coverage enabled"));
		BeginMaterialCoverageStrategy();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::BeginAutomatedCoverageStrategy - Material Coverage disabled, skipping"));
	}

	// 依次判断并开启 Niagara 覆盖
	if (Settings->bEnableNiagaraCoverage)
	{
		UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::BeginAutomatedCoverageStrategy - Niagara Coverage enabled"));
		BeginNiagaraCoverageStrategy();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::BeginAutomatedCoverageStrategy - Niagara Coverage disabled, skipping"));
	}

	// 显示监控 UI
	ShowMonitorUI();

	// 注册核心 Tick
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UPSOCacheManager::TickCoverage)
	);

	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::BeginAutomatedCoverageStrategy - Tick registered"));
}

void UPSOCacheManager::EndAutomatedCoverageStrategy()
{
	if (!bCoverageActive)
	{
		return;
	}

	// 关闭监控 UI
	HideMonitorUI();

	// 注销 Tick
	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	TickDelegateHandle = FTSTicker::FDelegateHandle();

	// 清理状态
	bCoverageActive = false;
	CoveragePhase = EPSOCoveragePhase::Material;
	CoverageCompleteElapsed = 0.0f;
	CurrentMaterialIndex = 0;
	CurrentNiagaraIndex = 0;
	CachedMaterials.Empty();
	CachedNiagaraSystems.Empty();
	CleanupCoverageActors();

	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::EndAutomatedCoverageStrategy - Tick unregistered, state cleared"));
}

bool UPSOCacheManager::TickCoverage(float DeltaTime)
{
	if (!bCoverageActive)
	{
		return false; // 自动注销 Tick
	}

	const UPSOCacheSettings* Settings = GetDefault<UPSOCacheSettings>();
	if (!Settings)
	{
		return false;
	}

	CoverageElapsedTotal += DeltaTime;
	UpdateMonitorUI();

	// ---- Phase: 材质覆盖 ----
	if (CoveragePhase == EPSOCoveragePhase::Material)
	{
		if (!Settings->bEnableMaterialCoverage || CachedMaterials.Num() == 0)
		{
			CoveragePhase = EPSOCoveragePhase::Niagara;
		}
		else
		{
			const FPSOMaterialCoverageConfig& MatCfg = Settings->MaterialCoverageConfig;
			const int32 Remaining = CachedMaterials.Num() - CurrentMaterialIndex;
			const int32 Count = FMath::Min(MatCfg.MaterialsPerFrame, Remaining);

			for (int32 i = 0; i < Count; ++i)
			{
				const int32 Idx = CurrentMaterialIndex + i;
				if (CachedMaterials.IsValidIndex(Idx))
				{
					ProcessMaterialForPSO(CachedMaterials[Idx], Idx);
				}
			}

			CurrentMaterialIndex += Count;

			if (CurrentMaterialIndex >= CachedMaterials.Num())
			{
				UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::TickCoverage - Material Coverage complete (%d materials)"),
					CachedMaterials.Num());
				CoveragePhase = EPSOCoveragePhase::Niagara;
				CurrentNiagaraIndex = 0;
			}
		}
	}

	// ---- Phase: Niagara 覆盖 ----
	if (CoveragePhase == EPSOCoveragePhase::Niagara)
	{
		if (!Settings->bEnableNiagaraCoverage || CachedNiagaraSystems.Num() == 0)
		{
			CoveragePhase = EPSOCoveragePhase::Complete;
			CoverageCompleteElapsed = 0.0f;
		}
		else
		{
			const FPSONiagaraCoverageConfig& NiagaraCfg = Settings->NiagaraCoverageConfig;
			const int32 Remaining = CachedNiagaraSystems.Num() - CurrentNiagaraIndex;
			const int32 Count = FMath::Min(NiagaraCfg.SystemsPerFrame, Remaining);

			for (int32 i = 0; i < Count; ++i)
			{
				const int32 Idx = CurrentNiagaraIndex + i;
				if (CachedNiagaraSystems.IsValidIndex(Idx))
				{
					ProcessNiagaraForPSO(CachedNiagaraSystems[Idx], Idx);
				}
			}

			CurrentNiagaraIndex += Count;

			if (CurrentNiagaraIndex >= CachedNiagaraSystems.Num())
			{
				UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::TickCoverage - Niagara Coverage complete (%d systems)"),
					CachedNiagaraSystems.Num());
				CoveragePhase = EPSOCoveragePhase::Complete;
				CoverageCompleteElapsed = 0.0f;
			}
		}
	}

	// ---- Phase: 全部完成（延迟关闭）----
	if (CoveragePhase == EPSOCoveragePhase::Complete)
	{
		const float Delay = Settings->CoverageCompleteDelaySeconds;
		CoverageCompleteElapsed += DeltaTime;

		if (CoverageCompleteElapsed >= Delay)
		{
			UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::TickCoverage - All coverage phases complete, waited %.1fs (config=%.1fs), stopping"),
				CoverageCompleteElapsed, Delay);
			EndAutomatedCoverageStrategy();

			// 自动退出游戏（启动参数 -psosysautoquitgame）
			if (bAutoQuitGame)
			{
				UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::TickCoverage - Auto-quit requested, exiting game..."));
				FPlatformMisc::RequestExit(false);
			}

			return false; // 注销 Tick
		}
	}

	return true; // 继续 Tick
}

void UPSOCacheManager::BeginMaterialCoverageStrategy()
{
	CurrentMaterialIndex = 0;
	CachedMaterials = GetAllMaterials();

	const UPSOCacheSettings* Settings = GetDefault<UPSOCacheSettings>();
	if (Settings)
	{
		const FPSOMaterialCoverageConfig& MatCfg = Settings->MaterialCoverageConfig;
		MaterialPoolCellSize = MatCfg.MaterialGridCellSize;
		MaterialActorPool.SetNum(MatCfg.MaterialActorPoolSize);
	}

	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::BeginMaterialCoverageStrategy - %d materials to process (pool capacity=%d)"),
		CachedMaterials.Num(), MaterialActorPool.Num());
}

void UPSOCacheManager::BeginNiagaraCoverageStrategy()
{
	CurrentNiagaraIndex = 0;
	CachedNiagaraSystems = GetAllNiagaraParticleSystems();

	const UPSOCacheSettings* Settings = GetDefault<UPSOCacheSettings>();
	if (Settings)
	{
		const FPSONiagaraCoverageConfig& NiagaraCfg = Settings->NiagaraCoverageConfig;
		NiagaraPoolCellSize = NiagaraCfg.NiagaraGridCellSize;
		NiagaraActorPool.SetNum(NiagaraCfg.NiagaraActorPoolSize);
		NiagaraCompPool.SetNum(NiagaraCfg.NiagaraActorPoolSize);
	}

	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::BeginNiagaraCoverageStrategy - %d systems to process (pool capacity=%d)"),
		CachedNiagaraSystems.Num(), NiagaraActorPool.Num());
}

void UPSOCacheManager::ProcessMaterialForPSO(const TSoftObjectPtr<UMaterial>& Material, int32 MaterialIndex)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UWorld* World = GameInstance->GetWorld();
	if (!World)
	{
		return;
	}

	// 加载材质资源
	UMaterial* LoadedMaterial = Material.LoadSynchronous();
	if (!LoadedMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("UPSOCacheManager::ProcessMaterialForPSO - Failed to load material: %s"),
			*Material.ToSoftObjectPath().ToString());
		return;
	}

	const int32 PoolSize = MaterialActorPool.Num();
	if (PoolSize == 0)
	{
		return;
	}

	// slot = MaterialIndex % PoolSize，从对象池中循环取用
	const int32 Slot = MaterialIndex % PoolSize;

	if (!MaterialActorPool.IsValidIndex(Slot))
	{
		return;
	}

	// 懒加载：slot 无 Actor 则 spawn
	if (!MaterialActorPool[Slot].IsValid())
	{
		const FVector SpawnPos = GetGridPosition(Slot, PoolSize, MaterialPoolCellSize);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = FName(*FString::Printf(TEXT("PSOMatActor_%d"), Slot));
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(),
			SpawnPos,
			FRotator::ZeroRotator,
			SpawnParams
		);

		// 给 Actor 挂上引擎默认的 Sphere 网格
		if (MeshActor)
		{
			UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
			if (MeshComp)
			{
				MeshComp->SetMobility(EComponentMobility::Movable);
				MeshComp->SetStaticMesh(SphereMesh);
			}
		}

		MaterialActorPool[Slot] = MeshActor;
	}

	AStaticMeshActor* MeshActor = MaterialActorPool[Slot].Get();
	UStaticMeshComponent* MeshComp = MeshActor ? MeshActor->GetStaticMeshComponent() : nullptr;
	if (MeshComp)
	{
		MeshComp->SetMaterial(0, LoadedMaterial);
	}

	UE_LOG(LogTemp, Verbose, TEXT("UPSOCacheManager::ProcessMaterialForPSO - Slot %d: %s"),
		Slot, *LoadedMaterial->GetName());
}

void UPSOCacheManager::ProcessNiagaraForPSO(const TSoftObjectPtr<UNiagaraSystem>& NiagaraSystem, int32 NiagaraIndex)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UWorld* World = GameInstance->GetWorld();
	if (!World)
	{
		return;
	}

	// 加载 Niagara 系统资源
	UNiagaraSystem* LoadedSystem = NiagaraSystem.LoadSynchronous();
	if (!LoadedSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UPSOCacheManager::ProcessNiagaraForPSO - Failed to load system: %s"),
			*NiagaraSystem.ToSoftObjectPath().ToString());
		return;
	}

	const int32 PoolSize = NiagaraActorPool.Num();
	if (PoolSize == 0)
	{
		return;
	}

	// slot = NiagaraIndex % PoolSize，从对象池中循环取用
	const int32 Slot = NiagaraIndex % PoolSize;

	if (!NiagaraActorPool.IsValidIndex(Slot) || !NiagaraCompPool.IsValidIndex(Slot))
	{
		return;
	}

	// 懒加载：slot 无 Actor/Component 则 spawn
	if (!NiagaraActorPool[Slot].IsValid() || !NiagaraCompPool[Slot].IsValid())
	{
		// Niagara Actor 位置在 Z 轴上偏移 CellSize 避免与材质 Actor 重叠
		const FVector SpawnPos = GetGridPosition(Slot, PoolSize, NiagaraPoolCellSize) + FVector(0.0f, 0.0f, NiagaraPoolCellSize);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = FName(*FString::Printf(TEXT("PSONiagaraActor_%d"), Slot));
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* TempActor = World->SpawnActor<AActor>(
			AActor::StaticClass(),
			SpawnPos,
			FRotator::ZeroRotator,
			SpawnParams
		);

		if (!TempActor)
		{
			return;
		}

		TempActor->SetRootComponent(NewObject<USceneComponent>(TempActor));

		UNiagaraComponent* NiagaraComp = NewObject<UNiagaraComponent>(TempActor, UNiagaraComponent::StaticClass());
		NiagaraComp->RegisterComponent();
		NiagaraComp->AttachToComponent(TempActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

		// 确保 Actor 在正确的网格位置
		TempActor->SetActorLocation(SpawnPos);

		NiagaraActorPool[Slot] = TempActor;
		NiagaraCompPool[Slot] = NiagaraComp;
	}

	UNiagaraComponent* NiagaraComp = NiagaraCompPool[Slot].Get();
	if (NiagaraComp)
	{
		// 复用 slot：停掉当前粒子，换上新系统
		NiagaraComp->Deactivate();
		NiagaraComp->SetAsset(LoadedSystem);
		NiagaraComp->Activate(true);
	}

	UE_LOG(LogTemp, Verbose, TEXT("UPSOCacheManager::ProcessNiagaraForPSO - Slot %d: %s"),
		Slot, *LoadedSystem->GetName());
}

void UPSOCacheManager::CleanupCoverageActors()
{
	UWorld* World = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		World = GameInstance->GetWorld();
	}

	if (World)
	{
		// 清理材质 Actor 对象池
		for (TWeakObjectPtr<AStaticMeshActor>& ActorPtr : MaterialActorPool)
		{
			if (ActorPtr.IsValid())
			{
				World->DestroyActor(ActorPtr.Get());
			}
		}

		// 清理 Niagara Actor 对象池
		for (TWeakObjectPtr<AActor>& ActorPtr : NiagaraActorPool)
		{
			if (ActorPtr.IsValid())
			{
				World->DestroyActor(ActorPtr.Get());
			}
		}
	}

	MaterialActorPool.Empty();
	NiagaraActorPool.Empty();
	NiagaraCompPool.Empty();
	MaterialPoolCellSize = 1000.0f;
	NiagaraPoolCellSize = 1000.0f;
}

FVector UPSOCacheManager::GetGridPosition(int32 SlotIndex, int32 PoolSize, float CellSize)
{
	const int32 GridCols = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(PoolSize))));
	const int32 Row = SlotIndex / GridCols;
	const int32 Col = SlotIndex % GridCols;
	return FVector(Col * CellSize, Row * CellSize, 0.0f);
}

// ---- 监控 UI ----

void UPSOCacheManager::ShowMonitorUI()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	APlayerController* PC = GameInstance->GetFirstLocalPlayerController();
	if (!PC)
	{
		return;
	}

	UGameViewportClient* ViewportClient = GameInstance->GetGameViewportClient();
	if (!ViewportClient)
	{
		return;
	}

	if (!MonitorWidget)
	{
		// 用标准 CreateWidget 确保 NativeConstruct 正确执行，WidgetTree 正确初始化
		MonitorWidget = CreateWidget<UPSOCacheMonitorWidget>(PC, UPSOCacheMonitorWidget::StaticClass());
	}

	if (MonitorWidget && !MonitorSlateWidget.IsValid())
	{
		MonitorSlateWidget = MonitorWidget->TakeWidget();
		ViewportClient->AddViewportWidgetContent(MonitorSlateWidget.ToSharedRef(), /*ZOrder=*/4000);
	}

	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::ShowMonitorUI - Monitor widget displayed (AddViewportWidgetContent)"));
}

void UPSOCacheManager::UpdateMonitorUI()
{
	if (!MonitorWidget)
	{
		return;
	}

	const UPSOCacheSettings* Settings = GetDefault<UPSOCacheSettings>();
	if (!Settings)
	{
		return;
	}

	FPSOCoverageDisplayData Data;
	Data.bActive = bCoverageActive;

	// 阶段
	switch (CoveragePhase)
	{
	case EPSOCoveragePhase::Material:
		Data.PhaseIndex = 0;
		Data.PhaseName = FText::FromString(TEXT("Material Coverage"));
		break;
	case EPSOCoveragePhase::Niagara:
		Data.PhaseIndex = 1;
		Data.PhaseName = FText::FromString(TEXT("Niagara Coverage"));
		break;
	case EPSOCoveragePhase::Complete:
		Data.PhaseIndex = 2;
		Data.PhaseName = FText::FromString(TEXT("Complete (Waiting...)"));
		break;
	}

	// 材质覆盖
	Data.bMaterialCoverageEnabled = Settings->bEnableMaterialCoverage;
	Data.MaterialTotal = CachedMaterials.Num();
	Data.MaterialProgress = FMath::Min(CurrentMaterialIndex, Data.MaterialTotal);
	if (Data.MaterialProgress < Data.MaterialTotal && CachedMaterials.IsValidIndex(Data.MaterialProgress))
	{
		Data.CurrentMaterialName = FText::FromString(CachedMaterials[Data.MaterialProgress].GetAssetName());
	}
	else
	{
		Data.CurrentMaterialName = FText::FromString(TEXT("-"));
	}

	// 统计对象池中已 spawn 的 Actor 数量
	Data.MaterialPoolUsed = 0;
	for (const TWeakObjectPtr<AStaticMeshActor>& Ptr : MaterialActorPool)
	{
		if (Ptr.IsValid()) { ++Data.MaterialPoolUsed; }
	}
	Data.MaterialPoolCapacity = MaterialActorPool.Num();

	// Niagara 覆盖
	Data.bNiagaraCoverageEnabled = Settings->bEnableNiagaraCoverage;
	Data.NiagaraTotal = CachedNiagaraSystems.Num();
	Data.NiagaraProgress = FMath::Min(CurrentNiagaraIndex, Data.NiagaraTotal);
	if (Data.NiagaraProgress < Data.NiagaraTotal && CachedNiagaraSystems.IsValidIndex(Data.NiagaraProgress))
	{
		Data.CurrentNiagaraSystemName = FText::FromString(CachedNiagaraSystems[Data.NiagaraProgress].GetAssetName());
	}
	else
	{
		Data.CurrentNiagaraSystemName = FText::FromString(TEXT("-"));
	}

	Data.NiagaraPoolUsed = 0;
	for (const TWeakObjectPtr<AActor>& Ptr : NiagaraActorPool)
	{
		if (Ptr.IsValid()) { ++Data.NiagaraPoolUsed; }
	}
	Data.NiagaraPoolCapacity = NiagaraActorPool.Num();

	// 配置
	Data.MaterialsPerFrame = Settings->MaterialCoverageConfig.MaterialsPerFrame;
	Data.SystemsPerFrame = Settings->NiagaraCoverageConfig.SystemsPerFrame;
	Data.MaterialCellSize = MaterialPoolCellSize;
	Data.NiagaraCellSize = NiagaraPoolCellSize;
	Data.CompleteDelaySeconds = Settings->CoverageCompleteDelaySeconds;

	// 时间
	Data.ElapsedTotal = CoverageElapsedTotal;
	Data.CompleteCountdown = FMath::Max(0.0f, Settings->CoverageCompleteDelaySeconds - CoverageCompleteElapsed);

	// PSO 缓存
	Data.bPSOCacheReady = bPSOCacheReady;
	Data.CachedPSOCount = CachedPSOCount;

	MonitorWidget->UpdateDisplay(Data);
}

void UPSOCacheManager::HideMonitorUI()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		if (UGameViewportClient* ViewportClient = GameInstance->GetGameViewportClient())
		{
			if (MonitorSlateWidget.IsValid())
			{
				ViewportClient->RemoveViewportWidgetContent(MonitorSlateWidget.ToSharedRef());
			}
		}
	}
	MonitorSlateWidget.Reset();
	MonitorWidget = nullptr;

	UE_LOG(LogTemp, Log, TEXT("UPSOCacheManager::HideMonitorUI - Monitor widget removed (RemoveViewportWidgetContent)"));
}



