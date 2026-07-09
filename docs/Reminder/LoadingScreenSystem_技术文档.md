# LoadingScreenSystem 插件技术文档

> **版本**: 1.0 | **作者**: xiele | **日期**: 2026-07-02
>
> 屏幕加载系统，包含黑屏过渡和关卡切换加载屏幕功能。

---

## 目录

1. [架构概览](#1-架构概览)
2. [核心子系统](#2-核心子系统)
3. [UI 控件层次](#3-ui-控件层次)
4. [配置系统](#4-配置系统)
5. [接口与扩展点](#5-接口与扩展点)
6. [多阶段加载进度机制](#6-多阶段加载进度机制)
7. [性能优化](#7-性能优化)
8. [输入拦截](#8-输入拦截)
9. [DataTable 关卡覆盖](#9-datatable-关卡覆盖)
10. [日志与调试](#10-日志与调试)
11. [类关系图](#11-类关系图)
12. [模块依赖](#12-模块依赖)

---

## 1. 架构概览

### 1.1 双系统设计

插件包含两套独立的加载界面系统，各自有独立的管理器和 Widget：

```
LoadingScreenSystem 插件
│
├── 关卡加载界面系统 (Level Loading Screen)
│   ├── ULevelLoadingManager           ← GameInstanceSubsystem 管理器
│   ├── ULevelLoadingScreenWidget      ← UMG Widget (背景图 + 视频 + 进度条)
│   ├── ILevelLoadingScreenInterface   ← 接口：对象可请求显示加载界面
│   └── FLevelLoadingScreenTableRow    ← DataTable：逐关卡覆盖配置
│
├── 黑屏加载界面系统 (Black Loading Screen)
│   ├── UBlackLoadingManager           ← GameInstanceSubsystem 管理器
│   ├── UBlackLoadingScreenWidget      ← UMG Widget (纯黑遮罩 + 淡入淡出)
│   ├── UBlackLoadingProcessTask       ← UObject 任务：控制黑屏生命周期
│   └── IBlackLoadingProcessInterface  ← 接口：外部处理器注册
│
└── 共享基础设施
    ├── ULoadingScreenWidget           ← 抽象基类（动画框架）
    ├── ULoadingScreenSettings         ← DeveloperSettings（统一配置）
    └── FLoadingScreenInputPreProcessor← IInputProcessor（输入拦截）
```

### 1.2 生命周期模型

两个管理器都继承自 `UGameInstanceSubsystem`，运行在 GameInstance 层面：

| 生命周期阶段 | 关卡加载管理器 | 黑屏加载管理器 |
|-------------|-------------|-------------|
| **创建条件** | 仅客户端（`!IsDedicatedServer`） | 仅客户端（`!IsDedicatedServer`） |
| **Initialize** | 订阅 `PreLoadMapWithContext` / `PostLoadMapWithWorld`，注册 Ticker | 注册 Ticker |
| **运行时** | 每帧 Tick：驱动进度计算 + 更新 UI 显示/隐藏 | 每帧 Tick：检查是否需要显示/隐藏 |
| **Deinitialize** | 移除输入拦截、清理 Widget、取消委托、移除 Ticker | 同左 |

---

## 2. 核心子系统

### 2.1 ULevelLoadingManager — 关卡加载管理器

**职责**：追踪关卡加载管线，显示带进度条的加载界面。

**核心数据流**：

```
OpenLevel 调用
    │
    ▼
PreLoadMap 回调 → HandlePreLoadMap()
    │   ├── 记录目标地图名 PreLoadMapName
    │   ├── 启动进度 Tick (bIsProgressTickEnabled = true)
    │   └── 进入 Preparing 阶段
    │
    ▼
[Preparing 阶段]  0% ~ 5%
    │   检测 GetAsyncLoadPercentage() >= 0 时自动推进
    ▼
[AsyncLoading 阶段]  5% ~ 70%
    │   使用引擎真实异步加载进度 GetAsyncLoadPercentage()
    ▼
PostLoadMap 回调 → HandlePostLoadMap()
    │   ├── 记录引擎加载耗时 LevelLoadingDuration
    │   └── 进入 WorldInit 阶段
    │
    ▼
[WorldInit 阶段]  70% ~ 95%
    │   用 EaseOut 曲线平滑估算进度
    │   超过最小显示时长后自动推进
    ▼
[Finalizing 阶段]  95% ~ 100%
    │   暂停游戏世界 (SetGamePaused)
    │   0.3 秒收尾后进入 Completed
    ▼
[Completed]  100%
    └── OnLoadingCompleted() → 恢复游戏、停止 Tick
```

**关键 API**：

| 方法 | 说明 |
|------|------|
| `GetPreciseLoadingProgress()` | 获取 0~100 精确进度（轮询模式，无广播） |
| `GetCurrentLoadingPhase()` | 获取当前加载阶段 |
| `GetRawAsyncLoadPercentage()` | 获取引擎原生异步加载百分比 |
| `IsCurrentlyLoadingMap()` | 是否处于 PreLoadMap/PostLoadMap 之间（含 DataTable 查表） |
| `GetCurrentLevelOverrideConfig()` | 获取当前关卡的覆盖配置 |
| `IsLevelLoadingScreenPersistent()` | 加载界面是否常驻打开 |

---

### 2.2 UBlackLoadingManager — 黑屏加载管理器

**职责**：管理纯黑遮罩显示，用于过渡动画期间的视觉遮挡。

**任务驱动的生命周期**：

```
外部调用者                     UBlackLoadingManager
    │                                │
    ├─ OpenBlackLoadingScreen() ────► 创建 UBlackLoadingProcessTask
    │   (Reason, bAutoClose)         注册到 External Processors 列表
    │                                │
    │                                ├─ Tick: CheckForAnyNeed() → 显示黑屏
    │                                │   ShowBlackLoadingScreen()
    │                                │   ├── 创建 Widget
    │                                │   ├── 开始淡入动画
    │                                │   └── 拦截输入
    │                                │
    │                                ├─ 动画完成回调
    │                                │   └── 若 bAutoClose → 自动反注册任务
    │                                │
    ├─ CloseBlackLoadingScreen() ───► UnregisterBlackLoadingProcessor()
    │   或 Task.Complete()            │
    │                                ├─ Tick: CheckForAnyNeed() = false
    │                                │   HideBlackLoadingScreen()
    │                                │   ├── 开始淡出动画
    │                                │   └── 动画完成后清理 Widget + 恢复输入
```

**两种使用模式**：

| 模式 | 示例场景 | API |
|------|---------|-----|
| **自动关闭** (`bAutoClose=true`) | 关卡加载动画过渡时遮挡 | `OpenBlackLoadingScreen(Reason, true)` |
| **手动控制** | 外部异步操作需要保持黑屏 | 创建 `UBlackLoadingProcessTask` → 完成时 `CompleteLoadingScreenProcessTask()` |

**外部处理器注册**：

```cpp
// 任何实现 IBlackLoadingProcessInterface 的对象都可以注册
BlackLoadingManager->RegisterBlackLoadingProcessor(MyProcessor);
// 当所有处理器都不再需要黑屏时，黑屏自动关闭
BlackLoadingManager->UnregisterBlackLoadingProcessor(MyProcessor);
```

---

## 3. UI 控件层次

### 3.1 类继承关系

```
UUserWidget
    └── ULoadingScreenWidget (Abstract)           ← 动画框架基类
            ├── ULevelLoadingScreenWidget         ← 关卡加载界面
            └── UBlackLoadingScreenWidget         ← 黑屏加载界面
```

### 3.2 ULoadingScreenWidget — 动画框架基类

提供统一的动画系统，子类只需实现 `StartLoadAnimation` / `StartUnloadAnimation` / `TickAnimation`：

```
状态机：
[None] ──StartLoadAnimation()──► [LoadAnimation] ──时间耗尽──► FinishLoadAnimation() → OnLoadAnimationCompleted 广播
[None] ──StartUnloadAnimation()──► [UnloadAnimation] ──时间耗尽──► FinishUnloadAnimation() → OnUnloadAnimationCompleted 广播
```

**动画驱动方式**：使用 `FTSTicker::GetCoreTicker()`，而非 `UWorld` TimerManager（关卡加载期间 World Timer 不工作）。

**安全设计**：Ticker Lambda 中使用 `TWeakObjectPtr` 捕获 Widget，防止悬空引用：

```cpp
TWeakObjectPtr<ULoadingScreenWidget> WeakThis(this);
TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
    FTickerDelegate::CreateLambda([WeakThis](float DeltaTime) -> bool
    {
        ULoadingScreenWidget* StrongThis = WeakThis.Get();
        if (!StrongThis) { return false; }  // Widget 已销毁，自动注销
        StrongThis->TickAnimation(DeltaTime);
        return true;
    }));
```

**BlueprintNativeEvent 扩展点**：

| 事件 | 用途 |
|------|------|
| `StartLoadAnimation` | 蓝图可覆盖：自定义加载动画 |
| `StartUnloadAnimation` | 蓝图可覆盖：自定义卸载动画 |
| `FinishLoadAnimation` | C++ 完成逻辑 + 蓝图可扩展 |
| `FinishUnloadAnimation` | C++ 完成逻辑 + 蓝图可扩展 |

**可绑定子控件** (`BindWidgetOptional`)：

| 控件名 | 类型 | 用途 |
|--------|------|------|
| `ProgressBar` | `UProgressBar` | 蓝图子类中按此名命名的进度条会自动绑定 |
| `LoadingText` | `UTextBlock` | 加载状态文本 |
| `BackgroundImage` | `UImage` | 背景图片 |

---

### 3.3 ULevelLoadingScreenWidget — 关卡加载界面

**Slate 控件层级**：

```
SOverlay (RootOverlay)                              ← 根节点
│
├── SOverlay (ProgressOverlay)                      ← 图片模式时可见
│   ├── SImage (BackgroundImageWidget)              ← 关卡背景图
│   └── WidgetTree 内容                              ← 蓝图子类控件（进度条等）
│
└── SOverlay (VideoOverlay)                         ← 视频模式时可见
    └── SCanvas (VideoImageCanvas)
        └── SImage (VideoImageWidget)               ← 视频占位/跳过图标
```

**内容类型（ELoadingScreenContentType）**：

| 类型 | 显示逻辑 | 可见层 |
|------|---------|--------|
| `Image` | 加载背景纹理 → 显示 ProgressOverlay | 进度层可见，视频层隐藏 |
| `Video` | 通过 `IMoviePlayer::SetupLoadingScreen` 播放视频 | 进度层隐藏，视频层可见 |

**配置解析优先级**（`ResolveConfig()`）：

```
DataTable 逐关卡覆盖 (bOverrideContent = true)
    ↓ 如果未覆盖
全局 Settings 配置
    ↓
PIE 下强制回退为 Image 类型（视频在 PIE 中不可用）
```

**加载动画与黑屏联动**：

关卡加载界面在播放动画时，会**自动打开黑屏过渡界面**进行遮挡：

```cpp
// 开始加载动画时：打开黑屏（自动关闭模式）
BlackMgr->OpenBlackLoadingScreen(TEXT("Level loading screen load animation"), /*bAutoClose=*/ true);

// 动画完成后：恢复进度层显示
ProgressOverlay->SetVisibility(EVisibility::Visible);
```

**进度平滑机制**：

进度条不会突变，而是按照**最小显示时长**进行线性平滑上限：

```cpp
// 进度 = min(引擎实际进度, 按时间推进的上限)
const float TimeBasedMax = (SmoothedProgressTime / MinimumDisplayTimeSecs) * 100.0f;
const float ClampedProgress = FMath::Min(RawProgress, TimeBasedMax);
```

---

### 3.4 UBlackLoadingScreenWidget — 黑屏加载界面

**Slate 控件层级**：

```
SOverlay (RootOverlay)
│
├── SOverlay (MaskOverlay)                          ← 黑屏遮罩层（带透明度动画）
│   └── SBorder (MaskBorder)                        ← 填充黑色
│       .BorderBackgroundColor = FLinearColor::Black
│
└── SOverlay (ContentOverlay)                       ← 蓝图内容层（上层）
    └── WidgetTree 内容                              ← 子类自定义控件
```

**动画实现**：通过修改 `MaskOverlay->SetRenderOpacity()` 实现渐入渐出：

| 动画阶段 | 起始透明度 | 结束透明度 |
|---------|-----------|-----------|
| LoadAnimation | 0.0（透明） | 1.0（不透明） |
| UnloadAnimation | 1.0（不透明） | 0.0（透明） |

---

## 4. 配置系统

### 4.1 ULoadingScreenSettings

继承自 `UDeveloperSettingsBackedByCVars`，在 **Project Settings → Game → Loading Screen** 中配置。

```ini
; Config/DefaultLoadingScreenSystem.ini
[/Script/LoadingScreenSystem.LoadingScreenSettings]
LevelLoadingScreenWidget=/LoadingScreenSystem/UI/WBP_LevelLoadingScreenWidget.WBP_LevelLoadingScreenWidget_C
BlackLoadingScreenWidget=/LoadingScreenSystem/UI/WBP_BlackLoadingScreenWidget.WBP_BlackLoadingScreenWidget_C
LevelLoadingScreenOverrideTable=/LoadingScreenSystem/Data/DT_LevelLoadingScreenOverride.DT_LevelLoadingScreenOverride
```

**Settings 类别**：

```
ULoadingScreenSettings (Config=LoadingScreenSystem, defaultconfig)
│
├── 关卡加载界面 (Category=LevelLoadingScreen)
│   ├── LevelLoadingScreenWidget          ← FSoftClassPath: Widget 类引用
│   ├── LevelLoadingScreenZOrder = 9000   ← 视口 Z 序
│   ├── LevelLoadingScreenContentType     ← Image / Video
│   ├── LevelLoadingScreenImageBackground ← 背景图资产
│   ├── LevelLoadingScreenVideoPath       ← 视频路径
│   ├── MinimumLevelLoadingScreenDisplayTime = 2.0f  ← 最小显示时长 (2~10s)
│   ├── HoldLevelLoadingScreenAdditionalSecsEvenInEditor
│   ├── LevelLoadingScreenHeartbeatHangDuration = 0.0
│   ├── LevelLoadingScreenHangDurationMultiplier = 1.0
│   └── LevelLoadingScreenOverrideTable   ← DataTable 路径
│
├── 黑屏加载界面 (Category=BlackLoadingScreen)
│   ├── BlackLoadingScreenWidget
│   ├── BlackLoadingScreenZOrder = 10000  ← 高于关卡加载界面
│   ├── BlackLoadingScreenLoadDuration = 0.15f   ← 淡入时长
│   ├── BlackLoadingScreenUnloadDuration = 1.0f  ← 淡出时长
│   ├── BlackLoadingScreenAnimationType   ← 动画类型
│   ├── BlackLoadingScreenAnimationMode   ← 缓动模式
│   ├── HoldBlackLoadingScreenAdditionalSecs = 2.0f
│   └── HoldBlackLoadingScreenAdditionalSecsEvenInEditor
│
└── 调试 (Category=Debug)
    ├── bForceLevelLoadingScreenVisible
    ├── bLogLoadingScreenReasonEveryFrame
    └── ForceTickLoadingScreenEvenInEditor
```

---

## 5. 接口与扩展点

### 5.1 ILevelLoadingScreenInterface

用于让 **GameState、PlayerController 及其组件** 声明需要显示关卡加载界面。

```cpp
class ILevelLoadingScreenInterface
{
    // 静态辅助方法：Cast 检查 + 调用虚函数
    static bool ShouldShowLevelLoadingScreen(UObject* TestObject, FString& OutReason);

    // 虚函数：子类覆盖以声明需求
    virtual bool ShouldShowLevelLoadingScreen(FString& OutReason) const { return false; }
};
```

**检查链**（`CheckForAnyNeedToShowLevelLoadingScreen()` 中的完整检查顺序）：

1. `bForceLevelLoadingScreenVisible` 强制显示
2. `IsCurrentlyLoadingMap()` 处于关卡加载流程
3. `WorldContext` 为空 / `World` 为空
4. `GameState` 未复制（null）
5. 加载界面常驻打开
6. `TravelURL` 非空（有待处理的旅行）
7. `PendingNetGame` 非空（正在连接服务器）
8. `!HasBegunPlay()` 世界未开始
9. `IsInSeamlessTravel()` 无缝旅行中
10. `ILevelLoadingScreenInterface::ShouldShow()` — GameState
11. `ILevelLoadingScreenInterface::ShouldShow()` — GameState 组件
12. `ILevelLoadingScreenInterface::ShouldShow()` — 每个 LocalPlayer 的 PC
13. `ILevelLoadingScreenInterface::ShouldShow()` — PC 的组件
14. 分屏模式检查

### 5.2 IBlackLoadingProcessInterface

用于让**任意 UObject** 注册为黑屏加载处理器。

```cpp
class IBlackLoadingProcessInterface
{
    // 静态辅助方法：Cast 检查 + 调用虚函数
    static bool ShouldShowLoadingScreen(UObject* TestObject, FString& OutReason);

    // 虚函数：返回 true 时黑屏保持显示
    virtual bool ShouldShowLoadingScreen(FString& OutReason) const { return false; }
};
```

**UBlackLoadingProcessTask** 是内置实现，同时支持蓝图调用：

| 蓝图方法 | 说明 |
|---------|------|
| `CreateBlackLoadingProcessTask` | 创建任务对象 → 自动注册到 Manager |
| `DestroyBlackLoadingProcessTask` | 销毁任务 → 取消注册 |
| `CompleteLoadingScreenProcessTask` | 标记完成 → 取消注册 |
| `SetShowLoadingScreenReason` | 设置调试原因描述 |

---

## 6. 多阶段加载进度机制

### 6.1 阶段定义（ELevelLoadingPhase）

| 阶段 | 触发条件 | 进度范围 | 计算方式 |
|------|---------|---------|---------|
| **Preparing** | `PreLoadMap` 回调 | 0% ~ 5% | 时间基线估算 (0.5s 内线性) |
| **AsyncLoading** | `GetAsyncLoadPercentage >= 0` | 5% ~ 70% | 引擎真实异步加载进度 |
| **WorldInit** | `PostLoadMap` 回调 | 70% ~ 95% | EaseOut 曲线 `1-(1-t)²` 平滑估算 |
| **Finalizing** | WorldInit 超过最小显示时长 | 95% ~ 100% | 0.3s 线性过渡 |
| **Completed** | Finalizing 0.3s 后 | 100% | — |

### 6.2 进度计算（CalculatePhaseProgress）

```cpp
// 各阶段权重（可配置）
PreparingWeight = 5.0;      //  0% ~  5%
AsyncLoadWeight = 65.0;     //  5% ~ 70%
WorldInitWeight  = 25.0;    // 70% ~ 95%
FinalizeWeight   = 5.0;     // 95% ~ 100%
//                    总计 = 100

// AsyncLoading 阶段直接使用引擎真实进度
return PreparingWeight + (GetAsyncLoadPercentage(NAME_None) / 100.0f) * AsyncLoadWeight;

// WorldInit 阶段使用 EaseOut 曲线（前快后慢，保证进度始终增长）
const float Smoothed = 1.0f - FMath::Square(1.0f - Ratio);
return PreparingWeight + AsyncLoadWeight + Smoothed * WorldInitWeight;
```

### 6.3 进度获取

采用**轮询模式**（无广播开销）：

```cpp
// Ticker 驱动 → 计算 → 缓存
CachedProgress = CalculatePhaseProgress();

// 外部轮询读取
float Progress = LevelLoadingManager->GetPreciseLoadingProgress();
```

屏幕调试信息（开发阶段）：

```cpp
GEngine->AddOnScreenDebugMessage(
    FString::Printf(TEXT("[LoadingProgress] Phase: %s | %.1f%%"),
        *UEnum::GetValueAsString(CurrentLoadingPhase), CachedProgress));
```

---

## 7. 性能优化

`ChangePerformanceSettings()` 在加载界面显示/隐藏时切换：

### 显示加载界面时（`bEnableLevelLoadingScreen = true`）

| 优化项 | 实现 |
|--------|------|
| **Shader 编译加速** | `FShaderPipelineCache::SetBatchMode(Fast)` — 优先编译当前需要的 Shader |
| **跳过世界渲染** | `GameViewportClient->bDisableWorldRendering = true` — 不渲染 3D 场景 |
| **关卡流式优先** | `WorldSettings->bHighPriorityLoadingLocal = true` — 优先流式加载当前关卡 |
| **挂起检测放宽** | `FThreadHeartBeat::SetDurationMultiplier(HangMultiplier)` — 避免误报卡死 |
| **暂停卡顿报告** | `FGameThreadHitchHeartBeat::SuspendHeartBeat()` — 加载期间不报告卡顿 |

### 隐藏加载界面时（恢复）

```cpp
FShaderPipelineCache::SetBatchMode(Background);        // 恢复后台编译
GameViewportClient->bDisableWorldRendering = false;    // 恢复渲染
WorldSettings->bHighPriorityLoadingLocal = false;       // 恢复普通优先级
FThreadHeartBeat::Get().SetDurationMultiplier(1.0);    // 恢复挂起检测
FGameThreadHitchHeartBeat::Get().ResumeHeartBeat();    // 恢复卡顿报告
```

### 额外保持时长

加载界面隐藏后再保持一段时间，给**纹理流式加载**留出时间，避免画面模糊：

```cpp
// 关卡加载界面：使用 MinimumLevelLoadingScreenDisplayTime
// 黑屏加载界面：使用 HoldBlackLoadingScreenAdditionalSecs (默认 2.0s)
if (TimeSinceScreenDismissed < HoldLoadingScreenAdditionalSecs) {
    bWantToForceShowLoadingScreen = true;  // 强制保持
}
```

---

## 8. 输入拦截

### FLoadingScreenInputPreProcessor

实现 `IInputProcessor` 接口，在加载界面显示期间拦截所有输入事件：

```cpp
class FLoadingScreenInputPreProcessor : public IInputProcessor
{
    bool CanEatInput() const { return !GIsEditor; }  // 编辑器中放行

    // 以下所有 Handle* 事件返回 CanEatInput() 结果
    // true = 吃掉事件，阻止传递到游戏层
    HandleKeyDownEvent()      → return CanEatInput();
    HandleKeyUpEvent()        → return CanEatInput();
    HandleMouseMoveEvent()    → return CanEatInput();
    HandleMouseButtonDownEvent() → return CanEatInput();
    // ... 等 9 个事件全部拦截
};
```

**注册与注销**（两个 Manager 各有独立实例）：

```cpp
// 注册（优先级 0，最高）
InputPreProcessor = MakeShareable<FLoadingScreenInputPreProcessor>(new ...);
FSlateApplication::Get().RegisterInputPreProcessor(InputPreProcessor, 0);

// 注销
FSlateApplication::Get().UnregisterInputPreProcessor(InputPreProcessor);
```

---

## 9. DataTable 关卡覆盖

### 9.1 行结构体（FLevelLoadingScreenTableRow）

```cpp
USTRUCT()
struct FLevelLoadingScreenTableRow : public FTableRowBase
{
    FSoftObjectPath LevelMap;                         // 关卡路径
    bool bShouldShowLevelLoadingScreen = false;       // 是否显示加载界面
    FLevelLoadingScreenOverrideConfig OverrideConfig; // 覆盖参数
};
```

### 9.2 覆盖参数（FLevelLoadingScreenOverrideConfig）

```cpp
USTRUCT()
struct FLevelLoadingScreenOverrideConfig
{
    bool bOverrideContent = false;                    // 是否覆盖内容
    ELoadingScreenContentType ContentType;            // Image / Video
    FSoftObjectPath ImageBackground;                  // 背景图资产
    FString VideoPath;                                // 视频路径
};
```

### 9.3 查表逻辑（FindLevelLoadingScreenTableRow）

- 使用 **懒加载缓存**：首次查表时 `TryLoad()` DataTable，后续复用
- 按 `LevelMap.GetAssetName()` 忽略大小写匹配
- 无匹配时：默认显示加载界面，使用全局 Settings 配置

---

## 10. 日志与调试

### 10.1 日志分类

```cpp
// 在 LogLoadingScreenSystem.h 中定义
DECLARE_LOG_CATEGORY_EXTERN(LogLevelLoading, Log, All);    // 关卡加载相关
DECLARE_LOG_CATEGORY_EXTERN(LogBlackLoading, Log, All);    // 黑屏加载相关
```

### 10.2 CSV Profiling

黑屏加载界面使用 CSV 事件来追踪性能：

```cpp
CSV_DEFINE_CATEGORY(BlackLoadingScreen, true);
CSV_EVENT(BlackLoadingScreen, TEXT("Show"));   // 显示事件
CSV_EVENT(BlackLoadingScreen, TEXT("Hide"));   // 隐藏事件
```

### 10.3 调试选项

| 选项 | 说明 |
|------|------|
| `bForceLevelLoadingScreenVisible` | 强制始终显示关卡加载界面 |
| `bLogLoadingScreenReasonEveryFrame` | 每帧输出显示/隐藏原因到日志 |
| `ForceTickLoadingScreenEvenInEditor` | 编辑器中也强制 Tick，便于迭代调试 |
| 命令行 `-NoLoadingScreen` | 完全禁止加载界面（非 Shipping 版本） |
| 屏幕调试信息 | 每帧在屏幕上显示阶段和进度百分比 |

---

## 11. 类关系图

```
┌──────────────────────────────────────────────────────────────────────┐
│                        ULoadingScreenSettings                        │
│                     (UDeveloperSettingsBackedByCVars)                 │
│   config = LoadingScreenSystem, defaultconfig                        │
│   CategoryName = "Game"                                              │
└────────────────────────────┬─────────────────────────────────────────┘
                             │ 引用
              ┌──────────────┼──────────────┐
              ▼              ▼              ▼
    FSoftClassPath   FSoftObjectPath   FSoftObjectPath
    (Widget 类)      (DataTable)       (背景图)

┌──────────────────────────────────────────────────────────────────┐
│                        GameInstance                                │
│  ┌──────────────────────────┐  ┌──────────────────────────────┐  │
│  │  ULevelLoadingManager    │  │   UBlackLoadingManager       │  │
│  │  (GameInstanceSubsystem) │  │   (GameInstanceSubsystem)    │  │
│  │                          │  │                              │  │
│  │  - 追踪地图加载进度       │  │  - 管理黑屏遮罩              │  │
│  │  - 管理关卡加载界面       │  │  - 外部处理器注册            │  │
│  │  - 委托回调              │  │  - 委托回调                  │  │
│  └───────────┬──────────────┘  └─────────────┬────────────────┘  │
│              │ 创建/管理                       │ 创建/管理         │
│              ▼                                ▼                   │
│  ┌───────────────────────┐    ┌──────────────────────────────┐   │
│  │ULevelLoadingScreenWidget│    │ UBlackLoadingScreenWidget   │   │
│  │  ├─ 背景图/视频         │    │  ├─ 黑屏遮罩 (MaskOverlay)   │   │
│  │  ├─ 进度条              │    │  └─ 蓝图内容层               │   │
│  │  └─ 蓝图内容层          │    │                              │   │
│  └────────────────────────┘    │  UBlackLoadingProcessTask    │   │
│                                │  ├─ IBlackLoadingProcess...  │   │
│  ┌───────────────────────┐    │  └─ 控制黑屏生命周期          │   │
│  │ILevelLoadingScreen...  │    └──────────────────────────────┘   │
│  │ (GameState/PC 组件)    │                                       │
│  └───────────────────────┘                                        │
│                                                                    │
│  ┌────────────────────────────────────────────────────────────┐   │
│  │            FLoadingScreenInputPreProcessor                  │   │
│  │            (IInputProcessor，两个 Manager 各一个实例)        │   │
│  └────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘

继承层次：
    UUserWidget
    └── ULoadingScreenWidget (Abstract)
        ├── ULevelLoadingScreenWidget
        └── UBlackLoadingScreenWidget

    UObject
    └── UBlackLoadingProcessTask (IBlackLoadingProcessInterface)
```

---

## 12. 模块依赖

### Build.cs

```cpp
// 公开依赖（头文件暴露的类型需要）
PublicDependencyModuleNames:
    Core, CoreUObject, Engine, Slate, SlateCore, UMG, DeveloperSettings

// 私有依赖（仅 .cpp 使用）
PrivateDependencyModuleNames:
    MoviePlayer,    // 视频播放 (IMoviePlayer)
    InputCore,      // 输入事件类型 (FKeyEvent 等)
    RenderCore,     // FShaderPipelineCache 性能切换
    PreLoadScreen,  // 引擎初始加载界面检测
```

### 对外接口

插件本身不定义独立的 Public/Private 模块分割，所有类型通过 `LOADINGSCREENSYSTEM_API` 宏导出。外部系统可依赖以下类：

| 类 | 用途 |
|----|------|
| `ULevelLoadingManager` | 获取加载进度、查表 |
| `UBlackLoadingManager` | 打开/关闭黑屏过渡 |
| `UBlackLoadingProcessTask` | 创建异步加载任务 |
| `ILevelLoadingScreenInterface` | 实现接口以触发加载界面 |
| `IBlackLoadingProcessInterface` | 实现接口以注册黑屏处理器 |
| `ULoadingScreenWidget` | 基类，用于自定义加载 Widget |
| `ULevelLoadingScreenWidget` | 基类，用于自定义关卡加载 Widget |
| `UBlackLoadingScreenWidget` | 基类，用于自定义黑屏 Widget |
| `ULoadingScreenSettings` | 获取配置值 |
| `FLevelLoadingScreenTableRow` | DataTable 行结构体 |

---

> **文档结束** — 基于 `LoadingScreenSystem` 插件源码 (2026-07-02 分析)
