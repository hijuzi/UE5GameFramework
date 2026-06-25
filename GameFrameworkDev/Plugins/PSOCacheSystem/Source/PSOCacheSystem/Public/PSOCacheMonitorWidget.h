// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "PSOCacheMonitorWidget.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * PSO 覆盖策略监控数据结构
 * 由 PSOCacheManager 每帧填充，传给 UI 刷新
 */
USTRUCT(BlueprintType)
struct PSOCACHESYSTEM_API FPSOCoverageDisplayData
{
	GENERATED_BODY()

	/** 覆盖策略是否激活 */
	bool bActive = false;

	/** 当前阶段：0=Material, 1=Niagara, 2=Complete */
	int32 PhaseIndex = 0;

	/** 阶段名称 */
	FText PhaseName;

	// ---- 材质覆盖 ----
	bool bMaterialCoverageEnabled = false;
	int32 MaterialProgress = 0;
	int32 MaterialTotal = 0;
	FText CurrentMaterialName;

	// ---- Niagara 覆盖 ----
	bool bNiagaraCoverageEnabled = false;
	int32 NiagaraProgress = 0;
	int32 NiagaraTotal = 0;
	FText CurrentNiagaraSystemName;

	// ---- 对象池 ----
	int32 MaterialPoolCapacity = 0;
	int32 MaterialPoolUsed = 0;
	int32 NiagaraPoolCapacity = 0;
	int32 NiagaraPoolUsed = 0;

	// ---- 配置 ----
	int32 MaterialsPerFrame = 0;
	int32 SystemsPerFrame = 0;
	float MaterialCellSize = 0.0f;
	float NiagaraCellSize = 0.0f;
	float CompleteDelaySeconds = 0.0f;

	// ---- 时间 ----
	float ElapsedTotal = 0.0f;
	float CompleteCountdown = 0.0f;

	// ---- PSO 缓存 ----
	bool bPSOCacheReady = false;
	int32 CachedPSOCount = 0;
};

/**
 * PSO 覆盖策略实时监控 UI
 * 自动化策略开始时显示，结束时关闭
 */
UCLASS()
class PSOCACHESYSTEM_API UPSOCacheMonitorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 刷新所有显示数据 */
	void UpdateDisplay(const FPSOCoverageDisplayData& Data);

protected:
	/** 创建一行标题+值文本 */
	UTextBlock* CreateInfoRow(UVerticalBox* Container, const FString& Label, const FString& InitialValue, FLinearColor ValueColor = FLinearColor::White);

	/** 创建分隔线 */
	void CreateSeparator(UVerticalBox* Container);

	/** 创建标题行 */
	UTextBlock* CreateTitle(UVerticalBox* Container, const FString& TitleText);

	// ---- 容器 ----
	UPROPERTY()
	UVerticalBox* RootBox;

	// ---- 标题和状态 ----
	UPROPERTY()
	UTextBlock* TextTitle;

	UPROPERTY()
	UTextBlock* TextStatus;

	// ---- 阶段 ----
	UPROPERTY()
	UTextBlock* TextPhase;

	// ---- 材质进度 ----
	UPROPERTY()
	UTextBlock* TextMaterialHeader;

	UPROPERTY()
	UTextBlock* TextMaterialProgress;

	UPROPERTY()
	UTextBlock* TextMaterialCurrent;

	// ---- Niagara 进度 ----
	UPROPERTY()
	UTextBlock* TextNiagaraHeader;

	UPROPERTY()
	UTextBlock* TextNiagaraProgress;

	UPROPERTY()
	UTextBlock* TextNiagaraCurrent;

	// ---- 对象池 ----
	UPROPERTY()
	UTextBlock* TextPoolHeader;

	UPROPERTY()
	UTextBlock* TextMatPool;

	UPROPERTY()
	UTextBlock* TextNiagaraPool;

	// ---- 配置 ----
	UPROPERTY()
	UTextBlock* TextConfigHeader;

	UPROPERTY()
	UTextBlock* TextConfig;

	// ---- 时间 ----
	UPROPERTY()
	UTextBlock* TextTime;

	// ---- PSO ----
	UPROPERTY()
	UTextBlock* TextPSOCache;
};
