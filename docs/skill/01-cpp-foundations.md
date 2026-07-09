# C++ 基础技能

UE C++ 开发的基石，涵盖项目配置、编码规范、反射系统、类型容器、模块构建、委托事件、日志和内存管理。

---

## project-structure

**适用于**：初次搭建项目或在现有项目中寻找 `.uproject`、`.Target.cs`、`Target.cs`、`Build.cs` 配置位置时；配置模块依赖、插件引用或调整 UBT 构建设置。

**核心内容**：项目的物理结构（`.uproject` 描述符）、`Source/` 布局约定、模块 `Build.cs` 文件、Target 文件、插件目录结构、`Config/` 下的 ini 层级、以及 Content 和 DataTable 源的典型位置。

---

## coding-standards

**适用于**：编写符合 Epic 规范的 UE C++ 代码 —— 类型前缀（U/A/F/E/I/T/S）、命名约定、`#include` 顺序、`UPROPERTY`/`UFUNCTION` 位置、代码风格约束。

**核心内容**：类名前缀规则、文件名约定、头文件包含顺序、const 正确性、auto 使用限制、注释规范。

---

## cpp-fundamentals

**适用于**：创建或编辑任何 UE C++ 类/结构体；将成员暴露给蓝图、编辑器或网络复制；诊断 UHT 编译错误；决定指针类型（TObjectPtr vs 裸指针 vs TWeakObjectPtr）。

**核心内容**：UObject 反射系统（UCLASS/USTRUCT/UENUM 宏）、UPROPERTY/UFUNCTION 说明符、GENERATED_BODY、CDO（类默认对象）、NewObject vs CreateDefaultSubobject、指针所有权层次。

---

## core-types-and-containers

**适用于**：选择正确的 UE 容器类型替代 STL；使用 FString/FName/FText 处理字符串；理解整数类型（int8/16/32/64）和数学类型。

**核心内容**：TArray（动态数组）、TMap（哈希表）、TSet（集合）、字符串三剑客（FString/FName/FText）、FVector/FRotator/FTransform 等数学类型、FDateTime/FTimespan。

---

## module-and-build-system

**适用于**：创建新模块、链接第三方库、配置 Build.cs、理解模块的加载阶段（Default/PreDefault/PostDefault）、选择 PublicDependencyModuleNames vs PrivateDependencyModuleNames。

**核心内容**：UBT 模块系统、Build.cs 配置项、模块 API 导出宏、模块启动/关闭生命周期、IModuleInterface、引擎模块类型（Runtime/Editor/Developer）。

---

## delegates-and-events

**适用于**：在 C++ 中连接回调和事件 —— 单播/多播委托、动态委托、事件分发器、以及绑定 Lambda/原始函数/UFUNCTION 的区别。

**核心内容**：DECLARE_DELEGATE 系列宏、单播委托、多播委托、动态委托（可与蓝图交互）、事件声明、AddDynamic/BindDynamic、Execute/ExecuteIfBound、Payload 数据。

---

## logging-and-assertions

**适用于**：添加结构化日志（UE_LOG）、自定义日志类别、运行时断言（check/verify/ensure）、以及 LogVerbosity 级别选择。

**核心内容**：DECLARE_LOG_CATEGORY_EXTERN、UE_LOG 带不同 Verbosity（Log/Warning/Error/Fatal）、check/checkf/verify/ensure/ensureMsgf、在编辑器和打包构建中的差异行为。

---

## memory-and-gc

**适用于**：管理 UObject 生命周期和普通 C++ 内存 —— 垃圾回收根集与集群、TObjectPtr vs TWeakObjectPtr、UPROPERTY GC 保护规则、内存泄漏诊断。

**核心内容**：GC 标记-清除算法、UPROPERTY 保护规则（不标记指针会悬空）、TObjectPtr 编辑器访问追踪、TWeakObjectPtr 非拥有引用、IsValid() vs 空指针检查、TSharedPtr/SharedRef 用于非 UObject。
