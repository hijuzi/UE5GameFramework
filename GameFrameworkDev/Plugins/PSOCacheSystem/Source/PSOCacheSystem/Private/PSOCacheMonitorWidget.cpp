// Copyright Epic Games, Inc. All Rights Reserved.

#include "PSOCacheMonitorWidget.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Blueprint/WidgetTree.h"

TSharedRef<SWidget> UPSOCacheMonitorWidget::RebuildWidget()
{
	// 防止重复构建（UE 可能在多处调用 RebuildWidget）
	if (RootBox)
	{
		return WidgetTree->RootWidget->TakeWidget();
	}

	// 根 CanvasPanel 用于定位到屏幕左上角
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;

	// 半透明背景边框
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Border->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
	Border->SetPadding(FMargin(16.0f, 12.0f));

	UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Border);
	CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
	CanvasSlot->SetPosition(FVector2D(20.0f, 20.0f));
	CanvasSlot->SetAutoSize(true);

	// 垂直布局容器
	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Border->SetContent(RootBox);

	// 构建 UI 元素
	TextTitle  = CreateTitle(RootBox, TEXT("PSO COVERAGE STRATEGY"));
	TextStatus = CreateInfoRow(RootBox, TEXT("Status:"), TEXT("Inactive"));
	CreateSeparator(RootBox);

	TextPhase = CreateInfoRow(RootBox, TEXT("Phase:"), TEXT("-"), FLinearColor::Yellow);
	CreateSeparator(RootBox);

	TextMaterialHeader  = CreateTitle(RootBox, TEXT("Material Coverage"));
	TextMaterialProgress = CreateInfoRow(RootBox, TEXT("Progress:"), TEXT("-"));
	TextMaterialCurrent  = CreateInfoRow(RootBox, TEXT("Current:"), TEXT("-"), FLinearColor(0.7f, 0.7f, 0.7f));
	CreateSeparator(RootBox);

	TextNiagaraHeader    = CreateTitle(RootBox, TEXT("Niagara Coverage"));
	TextNiagaraProgress  = CreateInfoRow(RootBox, TEXT("Progress:"), TEXT("-"));
	TextNiagaraCurrent   = CreateInfoRow(RootBox, TEXT("Current:"), TEXT("-"), FLinearColor(0.7f, 0.7f, 0.7f));
	CreateSeparator(RootBox);

	TextPoolHeader = CreateTitle(RootBox, TEXT("Actor Pool"));
	TextMatPool    = CreateInfoRow(RootBox, TEXT("Mat Pool:"), TEXT("-"));
	TextNiagaraPool = CreateInfoRow(RootBox, TEXT("Niagara Pool:"), TEXT("-"));
	CreateSeparator(RootBox);

	TextConfigHeader = CreateTitle(RootBox, TEXT("Config"));
	TextConfig = CreateInfoRow(RootBox, TEXT("Params:"), TEXT("-"), FLinearColor(0.6f, 0.8f, 1.0f));
	CreateSeparator(RootBox);

	TextTime = CreateInfoRow(RootBox, TEXT("Time:"), TEXT("-"), FLinearColor::Green);
	CreateSeparator(RootBox);

	TextPSOCache = CreateInfoRow(RootBox, TEXT("PSO Cache:"), TEXT("-"), FLinearColor(0.5f, 0.9f, 0.5f));

	// 返回根 Widget 的 Slate 表示
	return WidgetTree->RootWidget->TakeWidget();
}

void UPSOCacheMonitorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void UPSOCacheMonitorWidget::UpdateDisplay(const FPSOCoverageDisplayData& Data)
{
	if (!Data.bActive)
	{
		return;
	}

	// 状态
	if (Data.PhaseIndex == 2) // Complete
	{
		TextStatus->SetText(FText::FromString(TEXT("Finishing...")));
		TextStatus->SetColorAndOpacity(FLinearColor::Yellow);
	}
	else
	{
		TextStatus->SetText(FText::FromString(TEXT("Active")));
		TextStatus->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.2f));
	}

	// 阶段
	TextPhase->SetText(Data.PhaseName);

	// 材质进度
	if (Data.bMaterialCoverageEnabled && Data.MaterialTotal > 0)
	{
		const float Pct = (float)Data.MaterialProgress / (float)Data.MaterialTotal * 100.0f;
		TextMaterialProgress->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d  (%.1f%%)"), Data.MaterialProgress, Data.MaterialTotal, Pct)));
		TextMaterialCurrent->SetText(FText::Format(
			FText::FromString(TEXT("Current: {0}")), Data.CurrentMaterialName));
	}
	else
	{
		TextMaterialProgress->SetText(FText::FromString(
			Data.bMaterialCoverageEnabled ? TEXT("Done") : TEXT("Disabled")));
		TextMaterialCurrent->SetText(FText::FromString(TEXT("")));
	}

	// Niagara 进度
	if (Data.bNiagaraCoverageEnabled && Data.NiagaraTotal > 0)
	{
		const float Pct = (float)Data.NiagaraProgress / (float)Data.NiagaraTotal * 100.0f;
		TextNiagaraProgress->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d  (%.1f%%)"), Data.NiagaraProgress, Data.NiagaraTotal, Pct)));
		TextNiagaraCurrent->SetText(FText::Format(
			FText::FromString(TEXT("Current: {0}")), Data.CurrentNiagaraSystemName));
	}
	else
	{
		TextNiagaraProgress->SetText(FText::FromString(
			Data.bNiagaraCoverageEnabled ? TEXT("Done") : TEXT("Disabled")));
		TextNiagaraCurrent->SetText(FText::FromString(TEXT("")));
	}

	// 对象池
	TextMatPool->SetText(FText::FromString(
		FString::Printf(TEXT("Mat Pool:     %d / %d used"), Data.MaterialPoolUsed, Data.MaterialPoolCapacity)));
	TextNiagaraPool->SetText(FText::FromString(
		FString::Printf(TEXT("Niagara Pool: %d / %d used"), Data.NiagaraPoolUsed, Data.NiagaraPoolCapacity)));

	// 配置
	TextConfig->SetText(FText::FromString(
		FString::Printf(TEXT("Mat/Frame: %d  |  Niagara/Frame: %d  |  Cell: %.0f / %.0f cm  |  Delay: %.1fs"),
			Data.MaterialsPerFrame, Data.SystemsPerFrame,
			Data.MaterialCellSize, Data.NiagaraCellSize,
			Data.CompleteDelaySeconds)));

	// 时间
	if (Data.PhaseIndex == 2) // Complete 阶段倒计时
	{
		TextTime->SetText(FText::FromString(
			FString::Printf(TEXT("Elapsed: %.1fs  |  Countdown: %.1fs"), Data.ElapsedTotal, Data.CompleteCountdown)));
		TextTime->SetColorAndOpacity(FLinearColor::Yellow);
	}
	else
	{
		TextTime->SetText(FText::FromString(
			FString::Printf(TEXT("Elapsed: %.1fs"), Data.ElapsedTotal)));
		TextTime->SetColorAndOpacity(FLinearColor::Green);
	}

	// PSO 缓存
	TextPSOCache->SetText(FText::FromString(
		FString::Printf(TEXT("PSO Cache: %s  |  Count: %d"),
			Data.bPSOCacheReady ? TEXT("Ready") : TEXT("Not Ready"),
			Data.CachedPSOCount)));
}

UTextBlock* UPSOCacheMonitorWidget::CreateInfoRow(UVerticalBox* Container, const FString& Label, const FString& InitialValue, FLinearColor ValueColor)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(FString::Printf(TEXT("  %s  %s"), *Label, *InitialValue)));
	Text->SetColorAndOpacity(FSlateColor(ValueColor));

	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 14;
	Text->SetFont(Font);

	UVerticalBoxSlot* VSlot = Container->AddChildToVerticalBox(Text);
	VSlot->SetPadding(FMargin(0.0f, 2.0f));

	return Text;
}

void UPSOCacheMonitorWidget::CreateSeparator(UVerticalBox* Container)
{
	UTextBlock* Sep = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Sep->SetText(FText::FromString(TEXT("")));
	Sep->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f)));

	FSlateFontInfo Font = Sep->GetFont();
	Font.Size = 3;
	Sep->SetFont(Font);

	UVerticalBoxSlot* VSlot = Container->AddChildToVerticalBox(Sep);
	VSlot->SetPadding(FMargin(0.0f, 3.0f));
}

UTextBlock* UPSOCacheMonitorWidget::CreateTitle(UVerticalBox* Container, const FString& TitleText)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(TitleText));
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.7f, 1.0f)));

	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 18;
	Font.TypefaceFontName = TEXT("Bold");
	Text->SetFont(Font);

	UVerticalBoxSlot* VSlot = Container->AddChildToVerticalBox(Text);
	VSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 2.0f));

	return Text;
}
