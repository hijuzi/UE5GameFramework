---
name: level-loading-screen-table-config
overview: 创建 DataTable 行结构体 FLevelLoadingScreenTableRow，在 ULoadingScreenSettings 中新增 DataTable 配置，在 LevelLoadingManager 中实现查表逻辑，控制关卡加载界面是否显示以及提供覆盖配置。
todos:
  - id: add-table-row-struct
    content: 在 LoadingScreenInterface.h 中新增 FLevelLoadingScreenTableRow 结构体（继承 FTableRowBase）
    status: completed
  - id: add-settings-config
    content: 在 LoadingScreenSettings.h 中添加 DataTable 配置属性 LevelLoadingScreenOverrideTable
    status: completed
  - id: add-manager-methods
    content: 在 LevelLoadingManager.h 中声明 FindLevelLoadingScreenTableRow 和 GetCurrentLevelOverrideConfig 方法
    status: completed
    dependencies:
      - add-table-row-struct
  - id: implement-table-lookup
    content: 在 LevelLoadingManager.cpp 中实现查表逻辑、修改 CheckForAnyNeedToShowLevelLoadingScreen 的 bCurrentlyInLoadMap 分支、实现 GetCurrentLevelOverrideConfig
    status: completed
    dependencies:
      - add-table-row-struct
      - add-manager-methods
---

## 用户需求

为 LoadingScreenSystem 插件增加关卡级别加载界面显示控制能力，通过 DataTable 配置每个关卡是否显示加载界面以及对应的覆盖参数。

### 核心功能

- 新建 DataTable 行结构体 `FLevelLoadingScreenTableRow`，包含关卡路径、是否显示加载界面开关、以及关卡覆盖配置
- 在项目设置中可配置该 DataTable 的引用路径
- 关卡加载期间（`bCurrentlyInLoadMap` 为 true 时），根据当前关卡名查表决定是否显示加载界面
- 未在表中配置的关卡默认显示加载界面（保持向后兼容）
- 提供公共方法获取当前关卡的覆盖配置，供加载界面 Widget 使用以定制内容（视频、图片等）

## Tech Stack

- 语言：C++ (UE5)
- 模块：LoadingScreenSystem 插件
- 依赖：Core, CoreUObject, Engine（已有）

## 实现方案

### 整体策略

1. 在 `LoadingScreenSettings.h` 中新增 `FLevelLoadingScreenTableRow` 结构体（继承 `FTableRowBase`），同时添加 `#include "LoadingScreenInterface.h"` 以引用 `FLevelLoadingScreenOverrideConfig`
2. 在 `ULoadingScreenSettings` 中添加 `FSoftObjectPath` 类型的 DataTable 配置项
3. 在 `LevelLoadingManager` 中添加私有查表方法和公共覆盖配置获取方法
4. 修改 `CheckForAnyNeedToShowLevelLoadingScreen()` 中的查表逻辑

### 关键设计决策

- **关卡名匹配方式**：`PreLoadMapName` 存储的是短名称（如 `"M_5_Bioms_Showcase"`），而 `FSoftObjectPath::LevelMap` 是全路径。通过 `FSoftObjectPath::GetAssetName()` 提取短名称进行匹配
- **DataTable 加载时机**：在查表时按需加载（`TryLoad`），避免在 `Initialize()` 中同步加载造成启动延迟
- **默认行为**：表中未匹配到配置时默认返回 true（显示加载界面），保证现有关卡不受影响
- **遍历策略**：`FindRow` 需要行名，但 DataTable 行名不一定与关卡名一致。采用遍历 `GetRowMap()` 方式，匹配每行的 `LevelMap` 属性

### 核心目录结构

```
GameFrameworkDev/Plugins/LoadingScreenSystem/Source/LoadingScreenSystem/
├── Public/
│   ├── LoadingScreenSettings.h      # [MODIFY] 新增 FLevelLoadingScreenTableRow 结构体 + DataTable 配置属性
│   └── LevelLoading/
│       └── LevelLoadingManager.h    # [MODIFY] 新增查表方法声明
└── Private/
    └── LevelLoading/
        └── LevelLoadingManager.cpp  # [MODIFY] 实现查表逻辑 + 修改 CheckForAnyNeedToShowLevelLoadingScreen
```

### 关键代码结构

**FLevelLoadingScreenTableRow 结构体定义**

```cpp
USTRUCT(BlueprintType)
struct LOADINGSCREENSYSTEM_API FLevelLoadingScreenTableRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level", meta = (AllowedClasses = "/Script/Engine.World"))
    FSoftObjectPath LevelMap;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
    bool bShouldShowLevelLoadingScreen = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
    FLevelLoadingScreenOverrideConfig OverrideConfig;
};
```

**LevelLoadingManager 新增方法签名**

```cpp
// 私有：根据地图名称查表，返回匹配行（无匹配返回 nullptr）
FLevelLoadingScreenTableRow* FindLevelLoadingScreenTableRow(const FString& MapName) const;

public:
// 获取当前加载关卡的覆盖配置（未配置时返回默认空配置）
UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Loading|Config")
FLevelLoadingScreenOverrideConfig GetCurrentLevelOverrideConfig() const;
```