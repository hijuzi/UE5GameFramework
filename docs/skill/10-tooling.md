# 开发工具技能

自动化测试、调试技巧、编辑器脚本、打包部署、插件模块和性能优化。

---

## automation-and-testing

**适用于**：编写和运行自动化测试 —— Simple/Complex Automation Test（IMPLEMENT_SIMPLE_AUTOMATION_TEST / IMPLEMENT_COMPLEX_AUTOMATION_TEST）；Functional Test（AFunctionalTest）；Latent Action 测试；Gameplay 功能测试。

**核心内容**：Automation Test 宏（IMPLEMENT_SIMPLE_AUTOMATION_TEST / IMPLEMENT_COMPLEX_AUTOMATION_TEST）；FAutomationTestBase；Latent 命令（AddCommand / Latent Action）；AFunctionalTest 关卡内测试 Actor；Automation 预设（Session Frontend → Automation）；Gauntlet 框架（Headless 自动化运行）。

---

## debugging-techniques

**适用于**：调试 UE C++ 和游戏逻辑 —— 原生调试器使用（Visual Studio / Rider、natvis、Live Coding）；UE 可视化调试（DrawDebug 系列函数）；控制台命令调试；崩溃 Debug（CrashReporter / Minidump）；Log Visual Logger（游戏逻辑时间线录制）。

**核心内容**：DrawDebugLine/Sphere/Box/String 可视化调试；Visual Logger（UE_VLOG 宏）；Console Commands（stat unit/fps/game/slate 等）；stat 命令类别（StartFPSChart/StopFPSChart）；Debug HUD（ShowDebug）；Blueprint Debugger（断点/观察值/调用栈）；C++ 断点条件设置；Dump 分析。

---

## editor-scripting-and-python

**适用于**：使用 Python / Editor Utility Widget / Blutility 扩展编辑器 —— 批量处理资产、构建编辑器工具和可停靠 UMG 面板；无头 Python Commandlet（CI 批量脚本）；自定义 C++ 编辑器 API 暴露给 Python/Blueprint。

**核心内容**：Python `unreal` 模块 API；Editor Utility Widget（EUW，运行在编辑器的 UMG）；Editor Utility Blueprint（UEditorUtilityObject）；UEditorActorSubsystem / UEditorAssetSubsystem / ULevelEditorSubsystem；Python Commandlet（`-ExecutePythonScript`）；UFUNCTION CallInEditor / ScriptMethod / ScriptName 说明符。

---

## packaging-and-deployment

**适用于**：Cook、打包和发布项目 —— Cook 过程（by-the-book vs 增量）；Project Launcher（自定义 Launch Profile）；Pak 文件管理；平台特定设置（Windows/Android/iOS/Console）。

**核心内容**：UAT（Unreal Automation Tool）BuildCookRun；Project Launcher Profile 配置；Cook 设置（Cook by the book / only cook maps 列表）；Pak 文件（.pak 打包格式）；Staging Directory 阶段目录；平台 SDK 设置（Android NDK/SDK, iOS Cert）；Chunk 分块下载（Patching）；Shader Compilation Worker 管理。

---

## plugins-and-modules

**适用于**：创建、结构化管理 UE 插件 —— .uplugin 描述符；插件类型（Runtime/Editor）；模块依赖配置；插件 Content 资源路径；插件加载阶段。

**核心内容**：.uplugin 描述符字段（FriendlyName、Modules、Plugins、LoadingPhase）；插件 vs 项目模块 vs 引擎模块；LoadingPhase 顺序（Default/PreDefault/PostConfigInit/PostEngineInit）；模块启动/关闭（StartupModule/ShutdownModule）；插件资源引用路径（/PluginName/...）；插件 Content Only 模式。

---

## profiling-and-optimization

**适用于**：分析和优化性能 —— Unreal Insights（Trace System）；stat 命令；GPU Profiler；RenderDoc/PIX GPU 调试；Blueprint 性能分析；资产优化（纹理压缩/LOD/HLOD）。

**核心内容**：Unreal Insights（Trace channel、Timing View、Asset Loading）；stat 命令（stat unit/startfile/stopfile/game/gpu/scenerendering）；CSV Profiler；Blueprint Nativization 性能差异；RenderDoc/PIX 外部 GPU 调试器集成；纹理压缩格式（ASTC/ETC2/BC）；LOD 与 HLOD 自动生成；Level Streaming 性能（World Partition Data Layer）。
