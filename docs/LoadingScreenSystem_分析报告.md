# LoadingScreenSystem 插件架构分析报告

> **分析日期**: 2026-07-02  
> **分析方式**: 基于 unreal-engine-skills 知识库对照审查  
> **审查技能集合**: plugins-and-modules, project-structure, subsystems, memory-and-gc, coding-standards, umg-and-slate, blueprint-cpp-integration, levels-and-world-partition, timers-and-async, data-driven-design, module-and-build-system

---

## 插件概览

| 属性 | 值 |
|------|-----|
| **作者** | xiele |
| **版本** | 1.0 |
| **模块** | `LoadingScreenSystem` (Runtime, Default 加载阶段) |
| **子功能** | `LevelLoading` (关卡切换加载) + `BlackLoading` (黑屏过渡) |
| **源码规模** | 10 个 .h + 9 个 .cpp |
| **依赖模块** | Core, CoreUObject, Engine, Slate, SlateCore, UMG, DeveloperSettings (Public); MoviePlayer, InputCore, RenderCore, PreLoadScreen (Private) |

---

## 第一步：架构 & 模块设计

> 对照技能：`plugins-and-modules`, `project-structure`

### ✅ 优点

1. **插件结构规范** — 遵循标准插件布局 (`Source/` + `Content/` + `Config/` + `Resources/`)，目录清晰。
2. **Public/Private 分离到位** — 公共接口 (`LoadingScreenInterface.h`, `LoadingScreenWidget.h`) 放在 Public，实现细节放 Private。
3. **子功能目录划分** — `LevelLoading/` 和 `BlackLoading/` 独立子目录，职责边界明确。
4. **`.uplugin` 简洁** — 单一 Runtime 模块，无冗余字段。

### ⚠️ 发现的问题

| 问题 | 严重度 | 说明 |
|------|--------|------|
| **缺少 Editor 模块** | 低 | `ULoadingScreenSettings` 在 Runtime 模块中，但 `UDeveloperSettingsBackedByCVars` 在打包游戏中不需要。按 `plugins-and-modules` 技能建议，配置类可保留在 Runtime 中（`Editor` 模块在打包时被剥离）。当前设计没有问题，但如果将来有更多编辑器工具（如 DataTable 编辑器定制），建议拆分 `LoadingScreenSystemEditor` 模块。 |
| **`.uplugin` 缺少 `Plugins` 依赖声明** | 低 | 虽然模块通过 `Build.cs` 声明了代码依赖，但如果依赖了其他**插件**，应在 `.uplugin` 的 `"Plugins"` 数组中显式声明。当前项目依赖的模块（如 `DeveloperSettings`）是引擎内置模块，无需声明。 |
| **Config 目录下的 ini 文件名** | 提醒 | `DefaultLoadingScreenSystem.ini` 符合 `UCLASS(config=LoadingScreenSystem)` 的约定 ✅ |

### 评分：★★★★★ (5/5)
插件架构整体优秀，符合 Epic 最佳实践。

---

## 第二步：Subsystem 设计模式

> 对照技能：`subsystems`

### 关键发现：Manager 已经是 UGameInstanceSubsystem

```cpp
// LevelLoadingManager.h:58
class ULevelLoadingManager : public UGameInstanceSubsystem

// BlackLoadingManager.h:26
class UBlackLoadingManager : public UGameInstanceSubsystem
```

这与 `subsystems` 技能的建议**完全一致**。

### ✅ 做得好的地方

1. **正确的生命周期范围** — 两个 Manager 都继承 `UGameInstanceSubsystem`。加载界面需要跨关卡持久化，GameInstance 作用域是正确的选择（`subsystems` 技能明确：*"Cross-level session state → UGameInstanceSubsystem"*）。

2. **`ShouldCreateSubsystem` 排除了专用服务器**：
   ```cpp
   // LevelLoadingManager.cpp:80-86
   bool ULevelLoadingManager::ShouldCreateSubsystem(UObject* Outer) const
   {
       const UGameInstance* GameInstance = CastChecked<UGameInstance>(Outer);
       const bool bIsServerWorld = GameInstance->IsDedicatedServerInstance();
       return !bIsServerWorld;
   }
   ```
   按 `subsystems` 技能建议：**"Skip on dedicated server builds"**。无需 UI 的服务器端不创建加载界面管理器，节省资源 ✅。

3. **Initialize / Deinitialize 生命周期完整**：
   - `Initialize`：注册 Tick、订阅全局委托（`PreLoadMapWithContext`, `PostLoadMapWithWorld`）
   - `Deinitialize`：移除 Tick、取消委托、清理输入拦截 ✅

4. **使用 FTSTicker 而非 FTimerManager** — `subsystems` 技能指出 GameInstance 的 `Initialize` 阶段 `GetWorld()` 可能返回 null。这里采用 `FTSTicker::GetCoreTicker()` 绑定 Tick，生命周期可靠。

### ⚠️ 发现的问题

| 问题 | 严重度 | 建议 |
|------|--------|------|
| **未使用 `InitializeDependency`** | 低 | `LevelLoadingManager` 和 `BlackLoadingManager` 之间没有显式初始化顺序依赖。如果将来两者有依赖关系，应使用 `Collection.InitializeDependency<UBlackLoadingManager>()` |
| **`Super::Deinitialize()` 放在最后** | 好 | `LoadingScreenWidget.cpp` 中 `NativeDestruct` 是先清理再调 Super ✅。但 `LevelLoadingManager::Deinitialize()` 中 `Super::Deinitialize()` 也在最后 ✅ |

### 评分：★★★★★ (5/5)
Subsystem 模式运用正确，生命周期管理到位。

---

## 第三步：内存安全 & 编码规范

> 对照技能：`memory-and-gc`, `coding-standards`

### ✅ 做得好的地方

1. **UPROPERTY 保护完整** — 所有 UObject 指针成员都有对应的 `UPROPERTY()`：
   ```cpp
   UPROPERTY(Transient)
   TObjectPtr<UDataTable> CachedLevelLoadingScreenOverrideTable;  // ✅ Transient + TObjectPtr

   UPROPERTY()
   TObjectPtr<UBlackLoadingProcessTask> BlackLoadingProcessTask;  // ✅
   ```

2. **非 UObject 成员使用正确类型**：
   ```cpp
   TSharedPtr<SWidget> BlackLoadingScreenWidget;    // ✅ Widget 是 Slate 类型，非 UObject
   TSharedPtr<IInputProcessor> InputPreProcessor;    // ✅ Slate 框架要求 TSharedPtr
   ```

3. **Lambda 捕获安全** — `LoadingScreenWidget::StartTicker()` 中使用 `TWeakObjectPtr`：
   ```cpp
   TWeakObjectPtr<ULoadingScreenWidget> WeakThis(this);
   TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
       FTickerDelegate::CreateLambda([WeakThis](float DeltaTime) -> bool
       {
           ULoadingScreenWidget* StrongThis = WeakThis.Get();
           if (!StrongThis) { return false; }  // ✅ 对象已销毁时自动退出
           StrongThis->TickAnimation(DeltaTime);
           return true;
       }));
   ```
   这避免了 `memory-and-gc` 技能提到的最常见 bug：**"UObject* captured in a lambda by raw pointer"**。

4. **编码规范遵循良好**：
   - `#pragma once` ✅
   - `generated.h` 始终最后引入 ✅
   - 所有 UObject 指针使用 `TObjectPtr` ✅
   - bool 变量使用 `b` 前缀 (`bCurrentlyInLoadMap`, `bIsProgressTickEnabled`) ✅
   - 枚举使用 `uint8` 底层 (`UENUM(BlueprintType)`) ✅

### ⚠️ 发现的问题

| 问题 | 严重度 | 说明 |
|------|--------|------|
| **TickerHandle 无法从外部取消 Tick** | 中 | 两个 Manager 使用 `FTSTicker::GetCoreTicker().AddTicker()` 注册 Tick，会一直运行直到 `Deinitialize`。`BlackLoadingManager` 的 `Tick()` 被调用**每一帧**，即使不需要显示加载界面。虽然内部有 `bIsProgressTickEnabled` 开关（`LevelLoadingManager`），但 `BlackLoadingManager` 没有这个优化。好在每帧检查成本很低（只做几个 bool 判断）。 |
| **`UE_API` 宏技巧不必要** | 低 | `LoadingScreenInterface.h` 中定义了 `#define UE_API LOADINGSCREENSYSTEM_API` 用于接口的静态方法。虽然可以工作，但不是 UE 推荐的模式。建议直接在类上使用 `LOADINGSCREENSYSTEM_API` 宏，而不经过别名。 |
| **`LoadingScreenSettings.h` 包含非必要头文件** | 低 | `#include "Framework/Application/IInputProcessor.h"` 放在 Settings 头文件中（因为 `FLoadingScreenInputPreProcessor` 类在此声明）。输入拦截器的类定义可以移到单独文件，让 Settings 头文件更纯粹。 |

### 评分：★★★★★ (5/5)
GC 安全处理极好，编码风格符合 Epic 规范。

---

## 第四步：UMG/Slate & 蓝图集成

> 对照技能：`umg-and-slate`, `blueprint-cpp-integration`

### ✅ 做得好的地方

1. **`ULoadingScreenWidget` 基类设计合理**：
   - 继承 `UUserWidget` ✅
   - 使用 `BlueprintNativeEvent` 允许蓝图覆盖动画逻辑 ✅
   - `BlueprintAssignable` 委托 (`OnLoadAnimationCompleted`, `OnUnloadAnimationCompleted`) ✅

2. **动画系统使用 Ticker 驱动** — `umg-and-slate` 技能建议用 push 方式而非每帧属性绑定。此处动画确实需要每帧更新，Ticker 是正确的选择 ✅。

3. **Widget 生命周期管理正确**：
   ```cpp
   // NativeConstruct 启动 Ticker
   void ULoadingScreenWidget::NativeConstruct() { StartTicker(); }
   // NativeDestruct 停止 Ticker
   void ULoadingScreenWidget::NativeDestruct() { StopTicker(); }
   ```

4. **Manager 创建 Widget 的模式正确** — 通过 `CreateWidget<T>` 然后 `AddToViewport()`，符合 `umg-and-slate` 技能的标准模式 ✅。

### ⚠️ 发现的问题

| 问题 | 严重度 | 说明 |
|------|--------|------|
| **缺少 `BindWidget` 模式** | 中 | 加载界面 Widget 的 C++ 基类没有使用 `meta=(BindWidget)` 绑定具体的 UI 元素（如进度条、文本块）。所有 UI 交互都依赖蓝图子类通过动画委托回调。这在当前设计中可能够用，但限制了 C++ 层对 UI 元素的直接控制。建议添加 `BindWidgetOptional` 绑定来暴露进度条、提示文本等元素。 |
| **`ULoadingScreenWidget` 缺少 `BlueprintType`/`Blueprintable` 元数据** | 检查 | 虽然 UCLASS 标记了 `Abstract, BlueprintType, Blueprintable` ✅，但 `umg-and-slate` 技能建议加载界面 Widget 使用 `UCommonUserWidget`（CommonUI 插件）以获得更好的输入路由。当前项目未使用 CommonUI，这不算问题。 |
| **动画状态机简洁但缺少扩展性** | 低 | `ELoadingScreenAnimationState` 只有 3 个状态（None/Load/Unload），对当前需求够用。如果需要支持中途取消或暂停动画，需要扩展状态机。 |
| **`#define UE_API` 可能污染宏命名空间** | 低 | 在 `LoadingScreenInterface.h` 末尾有 `#undef UE_API`，处理正确 ✅。 |

### 评分：★★★★☆ (4/5)
UI 架构合理，动画系统清晰，建议添加 BindWidget 增强 C++ 控制力。

---

## 第五步：关卡加载核心 & 异步处理

> 对照技能：`levels-and-world-partition`, `timers-and-async`

### ✅ 亮点分析

这是整个插件最核心、设计最精良的部分：

1. **多阶段精确进度追踪** — `ELevelLoadingPhase` 将 OpenLevel 拆为 5 个阶段：
   ```
   Preparing(0-5%) → AsyncLoading(5-70%) → WorldInit(70-95%) → Finalizing(95-100%) → Completed
   ```
   这远比简单的 `GetAsyncLoadPercentage()` 更精确，因为引擎的异步加载进度只覆盖包加载阶段，不包括 World 初始化。

2. **全局映射委托订阅正确**：
   ```cpp
   FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ULevelLoadingManager::HandlePreLoadMap);
   FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULevelLoadingManager::HandlePostLoadMap);
   ```
   `levels-and-world-partition` 技能说明 PreLoadMap/PostLoadMap 适用于**所有关卡加载场景**（World Partition 和传统 Sublevel）✅。

3. **World Partition 兼容性** — 使用引擎级别的 `PreLoadMap` / `PostLoadMap` 委托而非直接操作 `ULevelStreaming`，对 World Partition 透明 ✅。

4. **`ShouldCreateSubsystem` 排除 DS** — 服务器不需要加载界面 UI ✅。

5. **最小显示时长 + 心跳挂起检测**：
   ```
   MinimumLevelLoadingScreenDisplayTime = 2.0s
   LevelLoadingScreenHeartbeatHangDuration = 0.0s (可配)
   ```
   加载界面至少显示 2 秒（防止闪烁），同时支持超时检测（防止永久卡死）。`timers-and-async` 技能推荐这种模式 ✅。

### ⚠️ 发现的问题

| 问题 | 严重度 | 说明 |
|------|--------|------|
| **异步加载未使用 `FStreamableManager`** | 低 | 当前进度追踪依赖引擎的 `GetAsyncLoadPercentage(NAME_None)` 全局异步进度。如果需要加载特定的非关卡资产（如加载界面自身的视频/图片），建议使用 `FStreamableManager::RequestAsyncLoad` 获得更精确的资产加载进度。不过当前的全局轮询方式对关卡加载场景是足够的。 |
| **WorldInit 阶段进度是估算值** | 低 | `WorldInit` 阶段的 70%-95% 基于时间平滑估算而非真实的 Actor 生成进度（UE 引擎未暴露此阶段的精确进度）。代码通过记录上一关卡的加载耗时来改进估算准确性，这是合理的折中方案 ✅。 |
| **黑屏加载 Manager 缺少进度开关** | 低 | `LevelLoadingManager` 有 `bIsProgressTickEnabled` 来在不需要时跳过 Tick，但 `BlackLoadingManager` 每帧都在 Tick。不过其 Tick 逻辑简单（几个 bool 检查），性能影响可忽略。 |

### 评分：★★★★★ (5/5)
多阶段进度追踪设计优秀，World Partition 兼容，心跳保护机制完善。

---

## 第六步：配置系统 & 模块依赖

> 对照技能：`data-driven-design`, `module-and-build-system`

### ✅ 做得好的地方

1. **`UDeveloperSettingsBackedByCVars` 正确使用** — `ULoadingScreenSettings` 继承自该类，自动出现在 Project Settings 面板中 ✅。`data-driven-design` 技能明确指出 **"Project Settings panel entry with change notifications → UDeveloperSettings"**。

2. **DataTable 驱动关卡配置** — `FLevelLoadingScreenTableRow` + `FLevelLoadingScreenOverrideConfig` 允许按关卡定制加载界面：
   ```
   Row Key → LevelMap (FSoftObjectPath)
           → bShouldShowLevelLoadingScreen (bool)
           → OverrideConfig (图片/视频)
   ```
   `data-driven-design` 技能推荐 **"Many rows of the same schema" → UDataTable** ✅。

3. **FSoftObjectPath 资产引用** — Widget 类、图片背景、DataTable 都使用软引用，避免在启动时强制加载 ✅。

4. **ini 配置文件正确** — `DefaultLoadingScreenSystem.ini` 使用 `[/Script/LoadingScreenSystem.LoadingScreenSettings]` 节头，路径格式正确 ✅。

### ⚠️ 发现的问题

| 问题 | 严重度 | 说明 |
|------|--------|------|
| **Build.cs 空数组冗余** | 低 | `PublicIncludePaths`, `PrivateIncludePaths`, `DynamicallyLoadedModuleNames` 都是空数组，可以删除以保持代码简洁。 |
| **RenderCore 依赖原因不明确** | 低 | `PrivateDependencyModuleNames` 包含 `RenderCore`。检查代码发现是在 `ChangePerformanceSettings()` 中操作 `FShaderPipelineCache`。`FShaderPipelineCache` 在 `Engine` 模块中声明，不需要 `RenderCore`。建议确认是否真的需要此依赖，或者可以移除。 |
| **PreLoadScreen 依赖** | 提醒 | 用于检测引擎预加载界面是否完成（`FPreLoadScreenManager`）。这是合理的依赖，但需要注意 `PreLoadScreen` 模块在 UE5 中已逐步被 `CommonStartupLoadingScreen` 替代。未来可能需要迁移。 |
| **缺少 `BlueprintReadWrite` 暴露** | 低 | `ULoadingScreenSettings` 使用 `UPROPERTY(config, EditAnywhere)` 但一些关键的运行时读取属性也标记了 `BlueprintReadWrite` 吗？需要验证。Settings 中大部分属性没有 `BlueprintReadWrite` 标记（使用 `config` + `EditAnywhere`），符合 `data-driven-design` 中 "Global programmer-near settings in .ini" 的模式 ✅。这是设计选择，不是 bug。 |

### 评分：★★★★☆ (4/5)
配置系统优秀，Build.cs 有少量冗余和待确认的依赖。

---

## 总结评分

| 维度 | 评分 | 关键优势 | 改进建议 |
|------|------|---------|---------|
| 1. 架构 & 模块设计 | ★★★★★ | 插件结构规范，目录划分清晰 | — |
| 2. Subsystem 模式 | ★★★★★ | GameInstanceSubsystem 选用正确，生命周期完整 | 考虑添加 InitializeDependency 声明顺序 |
| 3. 内存 & 编码规范 | ★★★★★ | GC 安全，Lambda 捕获安全，命名规范 | 清理 UE_API 宏别名 |
| 4. UMG & 蓝图集成 | ★★★★☆ | 动画系统设计好，Ticker 安全 | 添加 BindWidget 增强 C++ 控制力 |
| 5. 关卡加载核心 | ★★★★★ | 多阶段进度追踪优秀，WP 兼容 | 可考虑 FStreamableManager 精确加载 |
| 6. 配置 & 依赖 | ★★★★☆ | DataTable + DeveloperSettings 搭配好 | 清理 Build.cs 空数组，确认 RenderCore 依赖 |

**综合评分：4.7 / 5.0** — 一个设计精良、结构规范的 UE5 加载屏幕插件。

---

## 改进优先级

| 优先级 | 改进项 | 工作量 |
|--------|--------|--------|
| 🔴 高 | 确认/移除 `RenderCore` 依赖 | 10 分钟 |
| 🟡 中 | 为 Widget 基类添加 `BindWidgetOptional` 进度条绑定 | 30 分钟 |
| 🟡 中 | 清理 Build.cs 中的空数组 | 5 分钟 |
| 🟢 低 | 将 `FLoadingScreenInputPreProcessor` 移到独立文件 | 15 分钟 |
| 🟢 低 | 拆出 `LoadingScreenSystemEditor` 模块（如需编辑器工具） | 2 小时 |
| 🟢 低 | 评估迁移 `PreLoadScreen` → `CommonStartupLoadingScreen` | 1 天 |

---

*本报告由 unreal-engine-skills 知识库驱动生成*
