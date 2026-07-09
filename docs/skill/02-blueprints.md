# 蓝图系统技能

蓝图的视觉化编程基础以及 C++ 与蓝图的互操作方式。

---

## blueprint-fundamentals

**适用于**：理解 Blueprint 的本质 —— 决定 C++ 和蓝图各自负责什么逻辑；设计 C++ 基类 + 蓝图子类的架构；理解蓝图变量/函数/事件如何映射到 UPROPERTY/UFUNCTION。

**核心内容**：UBlueprint 编辑器资产 vs UBlueprintGeneratedClass 运行时类的二元架构；蓝图类型（Blueprint Class / Level BP / Interface / Macro Library / Function Library）；图类型（Event Graph / Functions / Construction Script / Macros）；变量可见性标志映射；SCS 组件树；Construction Script 幂等性；C++ 基类 + 蓝图子类的正确姿势。

---

## blueprint-cpp-integration

**适用于**：将 C++ 类、函数、属性暴露给蓝图 —— UFUNCTION 说明符（BlueprintCallable / BlueprintPure / BlueprintImplementableEvent / BlueprintNativeEvent）、UPROPERTY 蓝图访问控制、事件分发器绑定、TSubclassOf 使用、以及 C++/BP 边界的性能考量。

**核心内容**：BlueprintImplementableEvent（C++ 声明、BP 实现）vs BlueprintNativeEvent（C++ 默认实现、BP 可覆盖）；BlueprintPure vs BlueprintCallable；BlueprintAuthorityOnly；CallInEditor；ExposeOnSpawn；事件分发器的 Multicast 绑定；C++ 调用蓝图函数的正确方式；蓝图 VM 性能边界。
