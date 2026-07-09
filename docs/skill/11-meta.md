# 引擎导航技能

引擎源码定位与查阅方法。

---

## navigating-engine-source

**适用于**：在本地磁盘上的引擎源代码中定位、读取和引用精确的 Unreal Engine API；查找特定类的头文件路径、函数声明、UPROPERTY/UFUNCTION 定义；为 AI 助手提供准确的源码行号引用。

**核心内容**：引擎源码目录结构（Engine/Source/Runtime/ / Editor/ / Developer/ / Programs/）；关键头文件路径记忆（如 GameModeBase.h / Actor.h / UObjectGlobals.h）；API 查找策略（类名 → 头文件路径 → 行号定位）；版本注释中的行号漂移说明。
