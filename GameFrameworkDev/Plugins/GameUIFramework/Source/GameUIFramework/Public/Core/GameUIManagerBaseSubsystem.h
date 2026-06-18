// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPtr.h"

#include "GameUIManagerBaseSubsystem.generated.h"

#define UE_API GAMEUIFRAMEWORK_API

class FSubsystemCollectionBase;
class UGameUIPolicy;
class ULocalPlayer;
class UObject;

/**
 * 此管理器旨在被您的游戏的实际创建逻辑所替换，因此此类为抽象类以防止被直接实例化。
 * 
 * 如果您只需要基本功能，可以在自己的游戏中子类化此子系统。
 */
UCLASS(MinimalAPI, Abstract, config = GameUIFramework)
class UGameUIManagerBaseSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	UGameUIManagerBaseSubsystem() { }
	
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	UE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	
	const UGameUIPolicy* GetCurrentUIPolicy() const { return CurrentPolicy; }
	UGameUIPolicy* GetCurrentUIPolicy() { return CurrentPolicy; }

	UE_API virtual void NotifyPlayerAdded(ULocalPlayer* LocalPlayer);
	UE_API virtual void NotifyPlayerRemoved(ULocalPlayer* LocalPlayer);
	UE_API virtual void NotifyPlayerDestroyed(ULocalPlayer* LocalPlayer);

protected:
	UE_API void SwitchToPolicy(UGameUIPolicy* InPolicy);

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameUIPolicy> CurrentPolicy = nullptr;

	UPROPERTY(config, EditAnywhere)
	TSoftClassPtr<UGameUIPolicy> DefaultUIPolicyClass;
};

#undef UE_API
