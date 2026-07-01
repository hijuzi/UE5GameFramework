// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"

#include "LoadingScreenWidget.generated.h"

UENUM(BlueprintType)
enum class ELoadingScreenAnimationState : uint8
{
	None			UMETA(DisplayName = "无"),
	LoadAnimation	UMETA(DisplayName = "加载动画"),
	UnloadAnimation	UMETA(DisplayName = "卸载动画"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadingScreenWidgetAnimationCompleted);

UCLASS(Abstract, BlueprintType, Blueprintable)
class LOADINGSCREENSYSTEM_API ULoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULoadingScreenWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintNativeEvent, Category = "Loading Screen|Animation")
	void StartLoadAnimation();

	UFUNCTION(BlueprintNativeEvent, Category = "Loading Screen|Animation")
	void StartUnloadAnimation();

	UFUNCTION(BlueprintPure, Category = "Loading Screen|Animation")
	bool IsScreenAnimationPlaying() const;

	UPROPERTY(BlueprintAssignable, Category = "Loading Screen|Animation")
	FOnLoadingScreenWidgetAnimationCompleted OnLoadAnimationCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Loading Screen|Animation")
	FOnLoadingScreenWidgetAnimationCompleted OnUnloadAnimationCompleted;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintNativeEvent, Category = "Loading Screen|Animation")
	void FinishLoadAnimation();

	UFUNCTION(BlueprintNativeEvent, Category = "Loading Screen|Animation")
	void FinishUnloadAnimation();

	ELoadingScreenAnimationState AnimationState = ELoadingScreenAnimationState::None;

	float LoadAnimationDuration = 0.3f;

	float UnloadAnimationDuration = 0.3f;
    
	float AnimationElapsed = 0.0f;

	virtual void TickAnimation(float InDeltaTime);
private:

	FTSTicker::FDelegateHandle TickerHandle;
	void StartTicker();
	void StopTicker();
};
