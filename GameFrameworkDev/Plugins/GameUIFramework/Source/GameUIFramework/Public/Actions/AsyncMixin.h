// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "UObject/SoftObjectPtr.h"

#define UE_API GAMEUIFRAMEWORK_API

class FAsyncCondition;
class FName;
class UPrimaryDataAsset;
struct FPrimaryAssetId;
struct FStreamableHandle;
template <class TClass> class TSubclassOf;

DECLARE_DELEGATE_OneParam(FStreamableHandleDelegate, TSharedPtr<FStreamableHandle>)

/**
 * FAsyncMixin 允许更轻松地管理异步加载请求，确保请求按线性顺序处理，使代码编写更加简单。
 * 使用模式如下：
 *
 * 首先 - 继承自 FAsyncMixin，即使你是 UObject，也可以同时继承自 FAsyncMixin。
 *
 * 然后 - 你可以如下进行异步加载：
 * 
 * CancelAsyncLoading();			// 某些对象会被复用（如在列表中），因此取消任何待处理项很重要，以免它们意外完成。
 * AsyncLoad(ItemOne, CallbackOne);
 * AsyncLoad(ItemTwo, CallbackTwo);
 * StartAsyncLoading();
 * 
 * 你也可以安全地包含 'this' 作用域，这是 mix-in 的优势之一——所有回调都不会超出宿主 FAsyncMixin 派生对象的作用域。
 * 例如：
 * AsyncLoad(SomeSoftObjectPtr, [this, ...]() {
 *    
 * });
 *
 * 当所有异步加载请求完成后，将调用 OnFinishedLoading。
 * 
 * 如果你忘记调用 StartAsyncLoading()，我们会在下一帧自动调用它，但你应该在设置完成后记得调用它，
 * 因为可能所有资源已经加载完毕，这样可以避免一闪而过的加载指示器画面，那样很烦人。
 */
class FAsyncMixin : public FNoncopyable
{
protected:
	UE_API FAsyncMixin();

public:
	UE_API virtual ~FAsyncMixin();

protected:
	/** 当加载开始时调用。 */
	virtual void OnStartedLoading() { }
	/** 当所有加载完成时调用。 */
	virtual void OnFinishedLoading() { }

protected:
	/** 异步加载一个 TSoftClassPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftClassPtr<T> SoftClass, TFunction<void()>&& Callback)
	{
		AsyncLoad(SoftClass.ToSoftObjectPath(), FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 异步加载一个 TSoftClassPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftClassPtr<T> SoftClass, TFunction<void(TSubclassOf<T>)>&& Callback)
	{
		AsyncLoad(SoftClass.ToSoftObjectPath(),
			FSimpleDelegate::CreateLambda([SoftClass, UserCallback = MoveTemp(Callback)]() mutable {
				UserCallback(SoftClass.Get());
			})
		);
	}

	/** 异步加载一个 TSoftClassPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftClassPtr<T> SoftClass, const FSimpleDelegate& Callback = FSimpleDelegate())
	{
		AsyncLoad(SoftClass.ToSoftObjectPath(), Callback);
	}

	/** 异步加载一个 TSoftObjectPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftObjectPtr<T> SoftObject, TFunction<void()>&& Callback)
	{
		AsyncLoad(SoftObject.ToSoftObjectPath(), FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 异步加载一个 TSoftObjectPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftObjectPtr<T> SoftObject, TFunction<void(T*)>&& Callback)
	{
		AsyncLoad(SoftObject.ToSoftObjectPath(),
			FSimpleDelegate::CreateLambda([SoftObject, UserCallback = MoveTemp(Callback)]() mutable {
				UserCallback(SoftObject.Get());
			})
		);
	}

	/** 异步加载一个 TSoftObjectPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftObjectPtr<T> SoftObject, const FSimpleDelegate& Callback = FSimpleDelegate())
	{
		AsyncLoad(SoftObject.ToSoftObjectPath(), Callback);
	}

	/** 异步加载一个 FSoftObjectPath，完成后调用 Callback。 */
	UE_API void AsyncLoad(FSoftObjectPath SoftObjectPath, const FSimpleDelegate& Callback = FSimpleDelegate());

	/** 异步加载一组 FSoftObjectPath，完成后调用 Callback。 */
	void AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, TFunction<void()>&& Callback)
	{
		AsyncLoad(SoftObjectPaths, FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 异步加载一组 FSoftObjectPath，完成后调用 Callback。 */
	UE_API void AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, const FSimpleDelegate& Callback = FSimpleDelegate());

	/** 给定一组主资产，加载 LoadBundles 数组中指定的这些资产的属性所引用的所有 Bundle。 */
	template<typename T = UPrimaryDataAsset>
	void AsyncPreloadPrimaryAssetsAndBundles(const TArray<T*>& Assets, const TArray<FName>& LoadBundles, const FSimpleDelegate& Callback = FSimpleDelegate())
	{
		TArray<FPrimaryAssetId> PrimaryAssetIds;
		for (const T* Item : Assets)
		{
			PrimaryAssetIds.Add(Item);
		}

		AsyncPreloadPrimaryAssetsAndBundles(PrimaryAssetIds, LoadBundles, Callback);
	}

	/** 给定一组主资产 ID，加载 LoadBundles 数组中指定的这些资产的属性所引用的所有 Bundle。 */
	void AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& AssetIds, const TArray<FName>& LoadBundles, TFunction<void()>&& Callback)
	{
		AsyncPreloadPrimaryAssetsAndBundles(AssetIds, LoadBundles, FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 给定一组主资产 ID，加载 LoadBundles 数组中指定的这些资产的属性所引用的所有 Bundle。 */
	UE_API void AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& AssetIds, const TArray<FName>& LoadBundles, const FSimpleDelegate& Callback = FSimpleDelegate());

	/** 添加一个必须为真才能继续执行的未来条件。 */
	UE_API void AsyncCondition(TSharedRef<FAsyncCondition> Condition, const FSimpleDelegate& Callback = FSimpleDelegate());

	/**
	 * 不加载任何资源，仅将此回调插入到回调序列中，以便在异步加载完成时，
	 * 此事件会在序列的同一位置被调用。这在某些资源为可选时非常有用，
	 * 因为你不想将某个步骤与特定资产绑定。
	 */
	void AsyncEvent(TFunction<void()>&& Callback)
	{
		AsyncEvent(FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/**
	 * 不加载任何资源，仅将此回调插入到回调序列中，以便在异步加载完成时，
	 * 此事件会在序列的同一位置被调用。这在某些资源为可选时非常有用，
	 * 因为你不想将某个步骤与特定资产绑定。
	 */
	UE_API void AsyncEvent(const FSimpleDelegate& Callback);

	/** 刷新所有异步加载请求。 */
	UE_API void StartAsyncLoading();

	/** 取消所有待处理的异步加载。 */
	UE_API void CancelAsyncLoading();

	/** 当前是否有异步加载正在进行中？ */
	UE_API bool IsAsyncLoadingInProgress() const;

private:
	/**
	 * FLoadingState 是在一个大 Map 中为 FAsyncMixin 实际分配的内存，
	 * 这样 FAsyncMixin 自身不持有任何内存，我们只在需要时动态创建 FLoadingState，
	 * 并在不需要时销毁它。
	 */
	class FLoadingState : public TSharedFromThis<FLoadingState>
	{
	public:
		FLoadingState(FAsyncMixin& InOwner);
		virtual ~FLoadingState();

		/** 启动异步序列。 */
		void Start();

		/** 取消异步序列。 */
		void CancelAndDestroy();

		void AsyncLoad(FSoftObjectPath SoftObject, const FSimpleDelegate& DelegateToCall);
		void AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, const FSimpleDelegate& DelegateToCall);
		void AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& PrimaryAssetIds, const TArray<FName>& LoadBundles, const FSimpleDelegate& DelegateToCall);
		void AsyncCondition(TSharedRef<FAsyncCondition> Condition, const FSimpleDelegate& Callback);
		void AsyncEvent(const FSimpleDelegate& Callback);

		bool IsLoadingComplete() const { return !IsLoadingInProgress(); }
		bool IsLoadingInProgress() const;
		bool IsLoadingInProgressOrPending() const;
		bool IsPendingDestroy() const;

	private:
		void CancelOnly(bool bDestroying);
		void CancelStartTimer();
		void TryScheduleStart();
		void TryCompleteAsyncLoading();
		void CompleteAsyncLoading();

	private:
		void RequestDestroyThisMemory();
		void CancelDestroyThisMemory(bool bDestroying);

		/** 谁拥有此加载状态？我们需要它来回调所属的 mix-in 对象。 */
		FAsyncMixin& OwnerRef;

		/**
		 * 我们是否需要预加载 Bundle？如果我们没有预加载 Bundle（Bundle 需要你保持流式句柄，
		 * 否则它们会被销毁），那么在所有加载完成后可以安全地销毁 FLoadingState。
		 */
		bool bPreloadedBundles = false;

		class FAsyncStep
		{
		public:
			FAsyncStep(const FSimpleDelegate& InUserCallback);
			FAsyncStep(const FSimpleDelegate& InUserCallback, const TSharedPtr<FStreamableHandle>& InStreamingHandle);
			FAsyncStep(const FSimpleDelegate& InUserCallback, const TSharedPtr<FAsyncCondition>& InCondition);

			~FAsyncStep();

			void ExecuteUserCallback();

			bool IsLoadingInProgress() const
			{
				return !IsComplete();
			}

			bool IsComplete() const;
			void Cancel();

			bool BindCompleteDelegate(const FSimpleDelegate& NewDelegate);
			bool IsCompleteDelegateBound() const;

		private:
			FSimpleDelegate UserCallback;
			bool bIsCompletionDelegateBound = false;

			// 可能的异步'事物'
			TSharedPtr<FStreamableHandle> StreamingHandle;
			TSharedPtr<FAsyncCondition> Condition;
		};

		bool bHasStarted = false;

		int32 CurrentAsyncStep = 0;
		TArray<TUniquePtr<FAsyncStep>> AsyncSteps;
		TArray<TUniquePtr<FAsyncStep>> AsyncStepsPendingDestruction;

		FTSTicker::FDelegateHandle StartTimerDelegate;
		FTSTicker::FDelegateHandle DestroyMemoryDelegate;
	};

	UE_API const FLoadingState& GetLoadingStateConst() const;
	
	UE_API FLoadingState& GetLoadingState();

	UE_API bool HasLoadingState() const;

	UE_API bool IsLoadingInProgressOrPending() const;

private:
	static UE_API TMap<FAsyncMixin*, TSharedRef<FLoadingState>> Loading;
};

/**
 * 有时候使用 mix-in 并不合适。也许对象需要管理多个不同的任务，
 * 每个任务都有各自的异步依赖链/作用域。对于这些情况，可以使用 FAsyncScope。
 * 
 * 此类是一个独立的异步依赖处理器，让你可以发起多个加载任务并始终按正确顺序处理它们，
 * 就像将 FAsyncMixin 与你的类组合使用一样。
 */
class FAsyncScope : public FAsyncMixin
{
public:
	using FAsyncMixin::AsyncLoad;
	using FAsyncMixin::AsyncPreloadPrimaryAssetsAndBundles;
	using FAsyncMixin::AsyncCondition;
	using FAsyncMixin::AsyncEvent;
	using FAsyncMixin::CancelAsyncLoading;
	using FAsyncMixin::StartAsyncLoading;
	using FAsyncMixin::IsAsyncLoadingInProgress;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

enum class EAsyncConditionResult : uint8
{
	TryAgain,
	Complete
};

DECLARE_DELEGATE_RetVal(EAsyncConditionResult, FAsyncConditionDelegate);

/**
 * 异步条件允许你通过自定义原因来暂停异步加载，直到某个条件被满足。
 */
class FAsyncCondition : public TSharedFromThis<FAsyncCondition>
{
public:
	FAsyncCondition(const FAsyncConditionDelegate& Condition);
	FAsyncCondition(TFunction<EAsyncConditionResult()>&& Condition);
	virtual ~FAsyncCondition();

protected:
	bool IsComplete() const;
	bool BindCompleteDelegate(const FSimpleDelegate& NewDelegate);

private:
	bool TryToContinue(float DeltaTime);

	FTSTicker::FDelegateHandle RepeatHandle;
	FAsyncConditionDelegate UserCondition;
	FSimpleDelegate CompletionDelegate;

	friend FAsyncMixin;
};

#undef UE_API
