---
name: split-loading-screen-into-black-screen-and-umg
overview: 将 LoadingScreenManager 的显示逻辑拆分为双通道：ILoadingProcessInterface 控制原有 UMG LoadingScreenWidget，系统/引擎条件控制新增的 BlackScreen 简屏 Widget。两者独立显示/隐藏，互不干扰。
todos:
  - id: split-check-methods
    content: 在 LoadingScreenManager.cpp 中将 CheckForAnyNeedToShowLoadingScreen 拆分为 CheckForSystemNeedBlackScreen 和 CheckForAnyLoadingProcessInterfaceNeed
    status: completed
  - id: add-blackscreen-settings
    content: 在 CommonLoadingScreenSettings.h 中添加 BlackScreenWidget 和 BlackScreenZOrder 配置项
    status: completed
  - id: add-blackscreen-members
    content: 在 LoadingScreenManager.h 中添加 BlackScreen 相关成员变量和方法声明
    status: completed
  - id: implement-blackscreen-showhide
    content: 在 LoadingScreenManager.cpp 中实现 ShowBlackScreen/HideBlackScreen 及相关辅助方法
    status: completed
    dependencies:
      - add-blackscreen-members
      - split-check-methods
  - id: refactor-update-loadingscreen
    content: 重构 UpdateLoadingScreen 实现双通道独立管理逻辑
    status: completed
    dependencies:
      - implement-blackscreen-showhide
  - id: update-deinitialize
    content: 更新 Deinitialize 确保两个 Widget 和两个 InputPreProcessor 都被正确清理
    status: completed
    dependencies:
      - implement-blackscreen-showhide
---

## 用户需求

将 `LoadingScreenManager::CheckForAnyNeedToShowLoadingScreen()` 的所有判断条件按来源拆分为两个独立通道，实现双层遮罩架构：

### 通道拆分

1. **ILoadingProcessInterface 条件** → 控制原有的 LoadingScreenWidget（UMG 蓝图），包括 GameState 自身、GameState Components、ExternalLoadingProcessors、PlayerController 自身、PlayerController Components 的接口查询
2. **系统/引擎条件** → 控制新增的 BlackScreen 简屏 Widget，包括 CVar 强制显示、WorldContext/World/GameState 为空、LoadMap 中、TravelURL 未清、PendingNetGame、World 未 BeginPlay、SeamlessTravel、PlayerController 存在性检查

### 独立管理

- 两个 Widget 各自有独立的 Show/Hide 方法和状态标志
- 各自的 Show/Hide 过程中独立处理：Input Blocking、Performance Settings（世界渲染开关/Shader缓存模式/心跳检测等）、Delegate 广播
- BlackScreen 作为保底层，UMG LoadingScreen 在上层（通过 ZOrder 控制）
- `ForceLoadingScreenVisible` CVar 和 `HoldLoadingScreenAdditionalSecs` 归属 BlackScreen（系统层面）

## 技术方案

### 架构设计

当前 `ULoadingScreenManager` 只有一个决策通道和一个 Widget，重构后变为双通道架构：

```
ULoadingScreenManager::Tick()
    │
    ▼
UpdateLoadingScreen()
    ├── ShouldShowBlackScreen()          ← 系统条件
    │   └── CheckForSystemNeedBlackScreen()
    │       ├── CVars (ForceLoadingScreenVisible)
    │       ├── WorldContext/World/GameState 为空
    │       ├── LoadMap/TravelURL/PendingNetGame
    │       ├── World 未 BeginPlay / SeamlessTravel
    │       └── PlayerController 存在性检查
    │
    ├── handle → ShowBlackScreen() / HideBlackScreen()
    │
    ├── ShouldShowLoadingScreenWidget()  ← ILoadingProcessInterface 条件
    │   └── CheckForAnyLoadingProcessInterfaceNeed()
    │       ├── GameState (ILoadingProcessInterface)
    │       ├── GameState Components (ILoadingProcessInterface)
    │       ├── ExternalLoadingProcessors (ILoadingProcessInterface)
    │       ├── PlayerController (ILoadingProcessInterface)
    │       └── PlayerController Components (ILoadingProcessInterface)
    │
    └── handle → ShowLoadingScreenWidget() / HideLoadingScreenWidget()
```

### 改动文件清单

#### 1. `CommonLoadingScreenSettings.h` [MODIFY]

- 新增 `FSoftClassPath BlackScreenWidget` 配置项（BlackScreen UMG 蓝图类路径）
- 新增 `int32 BlackScreenZOrder` 配置项（默认 9000，低于 LoadingScreen 的 10000）

#### 2. `LoadingScreenManager.h` [MODIFY]

新增加成员：

- `bool bCurrentlyShowingBlackScreen` — BlackScreen 显示状态
- `TSharedPtr<SWidget> BlackScreenWidget` — BlackScreen 的 SWidget 引用
- `TSharedPtr<IInputProcessor> BlackScreenInputPreProcessor` — BlackScreen 的独立输入处理器
- `FString DebugReasonForBlackScreen` — BlackScreen 调试原因
- `double TimeBlackScreenShown` — BlackScreen 显示起始时间
- `double TimeBlackScreenLastDismissed` — BlackScreen 最后一次想关闭的时间
- `FOnLoadingScreenVisibilityChangedDelegate BlackScreenVisibilityChanged` — BlackScreen 可见性委托

新增方法：

- `CheckForSystemNeedBlackScreen()` — 系统条件检查（原 CheckForAnyNeedToShowLoadingScreen 的非 ILoadingProcessInterface 部分）
- `CheckForAnyLoadingProcessInterfaceNeed()` — ILoadingProcessInterface 条件检查
- `ShouldShowBlackScreen()` — BlackScreen 是否应显示（含 Hold 延迟逻辑）
- `ShouldShowLoadingScreenWidget()` — UMG LoadingScreen 是否应显示（仅 ILoadingProcessInterface）
- `ShowBlackScreen()` / `HideBlackScreen()` — BlackScreen 的显示/隐藏
- `ShowLoadingScreenWidget()` / `HideLoadingScreenWidget()` — 重命名或新增 UMG Widget 的显示/隐藏（与 BlackScreen 对称）
- `RemoveBlackScreenWidgetFromViewport()` — 移除 BlackScreen Widget
- `StartBlockingInputForBlackScreen()` / `StopBlockingInputForBlackScreen()` — BlackScreen 的独立输入拦截
- `ChangePerformanceSettingsForBlackScreen(bool)` — BlackScreen 的性能设置变更

注意：原有的 `ShowLoadingScreen()` / `HideLoadingScreen()` 保留但内部逻辑可能需要调整，确保不重复拦截输入和不重复修改性能设置。

#### 3. `LoadingScreenManager.cpp` [MODIFY]

**重构 `CheckForAnyNeedToShowLoadingScreen()`**：拆分为两个独立方法

`CheckForSystemNeedBlackScreen()`（约 40 行）：

- CVar ForceLoadingScreenVisible
- Context == nullptr / World == nullptr / GameState == nullptr
- bCurrentlyInLoadMap
- !TravelURL.IsEmpty()
- PendingNetGame != nullptr
- !World->HasBegunPlay()
- World->IsInSeamlessTravel()
- Splitscreen 缺少 PC / 非 Splitscreen 无 PC

`CheckForAnyLoadingProcessInterfaceNeed()`（约 20 行）：

- GameState (ILoadingProcessInterface)
- GameState->GetComponents() 遍历
- ExternalLoadingProcessors 遍历
- LocalPlayers → PlayerController (ILoadingProcessInterface)
- LocalPlayers → PlayerController->GetComponents() 遍历

**新增 `ShouldShowBlackScreen()`**：参照原 `ShouldShowLoadingScreen()`，包含 `bCmdLineNoLoadingScreen` 检查（对 BlackScreen 也生效）、GameViewportClient 空检查、`HoldLoadingScreenAdditionalSecs` 延迟逻辑。

**新增显示/隐藏方法**：`ShowBlackScreen()` / `HideBlackScreen()` 完全独立管理：

- 独立的 `bCurrentlyShowingBlackScreen` 状态
- 独立的 InputPreProcessor（另一份 `FLoadingScreenInputPreProcessor` 实例）
- 独立的 Widget 创建（从 `BlackScreenWidget` 配置加载 UMG 类，失败时 fallback 到纯黑 SColorBlock）
- 独立的 Viewport 添加/移除（使用 `BlackScreenZOrder`）
- 独立的 `ChangePerformanceSettings` 调用
- 独立的 `BlackScreenVisibilityChanged` 委托广播
- 独立的 FThreadHeartBeat 管理

**修改 `UpdateLoadingScreen()`**：分别调用两组 ShouldShow/Show/Hide：

```
UpdateLoadingScreen()
    ├── if (ShouldShowBlackScreen()) ShowBlackScreen() else HideBlackScreen()
    └── if (ShouldShowLoadingScreenWidget()) ShowLoadingScreenWidget() else HideLoadingScreenWidget()
```

### 关键设计决策

1. **命名区分**：原 `ShowLoadingScreen()`/`HideLoadingScreen()` 重命名为 `ShowLoadingScreenWidget()`/`HideLoadingScreenWidget()`，与 `ShowBlackScreen()`/`HideBlackScreen()` 对称清晰
2. **ZOrder 差 1000**：BlackScreen ZOrder 默认 9000，LoadingScreen ZOrder 默认 10000，确保 UMG 界面在 BlackScreen 之上
3. **BlackScreen 是纯黑遮罩**：当 BlackScreen Widget 类未配置或加载失败时，fallback 到 `SNew(SColorBlock)` 纯黑，保证在引擎加载阶段无闪白/无闪黑
4. **`NoLoadingScreen` 命令行参数对两者都生效**：开发者调试时可以统一关闭所有遮罩
5. **`ExternalLoadingProcessors` 保留不变**：`RegisterLoadingProcessor`/`UnregisterLoadingProcessor` 接口不变，`ULoadingProcessTask` 依然注册到该列表，但它现在只影响 UMG LoadingScreen，不影响 BlackScreen

### 性能考量

- 每帧 Tick 中，两个 `CheckFor*` 方法只做轻量布尔判断和指针检查，无额外开销
- BlackScreen 在稳定状态下（World 正常、BeginPlay 后、PC 存在、无 Travel）会很快关闭，不会一直显示
- 两个 InputPreProcessor 互不影响，各自在 Tick 中吃输入