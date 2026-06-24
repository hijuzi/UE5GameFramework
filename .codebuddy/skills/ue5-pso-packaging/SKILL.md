---
name: ue5-pso-packaging
description: >
  UE5 PSO（Pipeline State Object）缓存收集、转换和集成打包技能。
  当用户提到 UE5 打包、PSO 缓存、Pipeline State Object、shader 编译卡顿、
  .rec.upipelinecache、.spc、.shk、ShaderPipelineCacheTools expand、
  Shader 稳定键、稳定 PSO 缓存、内建 PSO 缓存打包等关键词时应触发此技能。
  适用场景：UE5 项目需要消除运行时着色器编译卡顿，将 PSO 缓存预编译并打入包体。
---

# UE5 PSO 打包技能

## 核心规则（必须严格遵守）

### 路径约束规则

1. **绝对禁止硬编码路径**。所有路径（引擎路径、项目路径、打包输出路径、PSO 缓存路径）
   必须从本技能目录下的 `config.json` 文件读取。
2. 执行任何步骤前，第一步永远是读取 `config.json` 并解析所有路径字段。
3. 如果 `config.json` 中某个必要字段缺失，**立即上报问题**，不得猜测或使用默认值。
4. 命令执行时使用 `config.json` 中的绝对路径拼接，不依赖当前工作目录。

### 问题上报规则（最高优先级）

执行过程中遇到任何异常情况，**必须**按以下流程处理：

1. **立即暂停**当前步骤，不得继续执行后续操作。
2. **上报问题**，包含以下五项信息：
   - **当前步骤**：Step X — 步骤名称
   - **问题描述**：什么操作出现了什么问题
   - **预期结果**：正常情况下应该是什么样
   - **实际结果**：实际发生了什么（包含错误信息、日志片段、文件路径等）
   - **需要用户决策**：列出用户需要选择或提供的修正方案选项
3. **等待用户回复**明确指示后，才能继续下一步。
4. **严禁**自行修复问题、绕过问题、跳过步骤、或假设用户意图。

### 步骤结果汇报规则

每步执行完成后，必须汇报：
- 步骤编号和名称
- 执行结果（成功/失败/警告）
- 关键输出信息（生成的文件路径、命令输出摘要等）

---

## 前置条件：加载配置文件

每个步骤开始前，确认已从 `config.json` 加载以下路径变量：

| 变量 | config.json 路径 | 当前值 |
|------|-----------------|--------|
| `ue5_install_dir` | `ue5.install_dir` | 从 config.json 读取 |
| `editor_cmd` | `ue5.editor_cmd_path` | 从 config.json 读取 |
| `uat_bat` | `ue5.uat_bat_path` | 从 config.json 读取 |
| `project_dir` | `project.project_dir` | 从 config.json 读取 |
| `uproject_file` | `project.uproject_file` | 从 config.json 读取 |
| `project_name` | `project.project_name` | 从 config.json 读取 |
| `shader_formats` | `project.shader_formats` | 从 config.json 读取 |
| `package_output_dir` | `package.output_dir` | 从 config.json 读取 |
| `pso_cache_dir` | `pso.cache_work_dir` | 从 config.json 读取 |
| `shk_source_rel` | `pso.shk_source_relative` | 从 config.json 读取 |
| `rec_source_rel` | `pso.rec_source_relative` | 从 config.json 读取 |
| `spc_target_rel` | `pso.spc_target_relative` | 从 config.json 读取 |

---

## Step 0：加载并验证 config.json

**需要的 config.json 字段**：全部

**操作**：
1. 读取本技能目录下的 `config.json` 文件。
   config.json 路径为 `<skill_dir>/config.json`，其中 `<skill_dir>` 是 SKILL.md 所在目录。
2. 解析所有字段，确认每个字段都存在且值不为空。
3. 验证以下关键路径在文件系统上实际存在（使用工具检查）：
   - `ue5.install_dir`
   - `ue5.editor_cmd_path`
   - `ue5.uat_bat_path`
   - `project.project_dir`
   - `project.uproject_file`

**验证检查点**：
- config.json 是否存在且格式正确
- 所有必需字段是否非空
- 关键路径是否在文件系统上存在

**失败处理**：
- config.json 不存在 → 上报：文件路径，询问是否创建
- 某个字段缺失 → 上报：缺失字段名，询问用户补充
- 路径不存在 → 上报：不存在的路径，询问用户修正

---

## Step 1：验证项目 PSO 配置

**需要的 config.json 字段**：`project.project_dir`、`project.project_name`

**操作**：
1. 读取 `{project_dir}/Config/DefaultEngine.ini`，检查以下配置项：
   - `[DevOptions.Shaders]` 下的 `NeedsShaderStableKeys=true`
   - `[ConsoleVariables]` 下的 `r.ShaderPipelineCache.Enabled=1`
   - `[ConsoleVariables]` 下的 `r.ShaderPipelineCache.LogPSO=1`
   - `[ConsoleVariables]` 下的 `r.ShaderPipelineCache.SaveUserCache=1`
   - `[ConsoleVariables]` 下的 `r.ShaderPipelineCache.BackgroundBatchSize=1`
   - `[ConsoleVariables]` 下的 `r.ShaderPipelineCache.GameFileMaskEnabled=1`

2. 读取 `{project_dir}/Config/DefaultGame.ini`，检查以下配置项：
   - `[/Script/Engine.RendererSettings]` 下的 `bSharedMaterialNativeLibraries=true`
   - `[/Script/Engine.RendererSettings]` 下的 `bShareMaterialShaderCode=true`

**验证检查点**：
- 列出缺失或值不正确的配置项清单

**失败处理**：
- 配置缺失 → 上报具体缺失项和所在文件，等待用户确认如何修改
- 文件不存在 → 上报文件路径，等待用户指示

---

## Step 2：首次打包项目（生成 Cook 产物）

**需要的 config.json 字段**：
- `ue5.uat_bat_path`
- `project.uproject_file`
- `project.project_name`
- `project.target_platform`
- `package.output_dir`

**操作**：
使用 RunUAT 执行 BuildCookRun 命令打包项目：

```
"{uat_bat}" BuildCookRun `
  -project="{uproject_file}" `
  -platform={target_platform} `
  -clientconfig=Development `
  -build -cook -stage -pak -archive `
  -archivedirectory="{package_output_dir}"
```

**参数说明**：
- `-build`：编译 C++ 代码
- `-cook`：Cook 资源（此步会生成 .shk 稳定键文件到 `Saved/Cooked/.../PipelineCaches/`）
- `-stage`：将 Cook 后的资源拷贝到 staging 目录
- `-pak`：打包为 .pak 文件
- `-archive`：归档到指定输出目录

**验证检查点**：
- UAT 命令是否成功退出（exitCode=0）
- 打包输出目录是否存在 .exe 文件
- `Saved/Cooked/Windows/{project_name}/Metadata/PipelineCaches/` 下是否有 .shk 文件

**失败处理**：
- Cook 失败 → 上报 UAT 输出日志中的错误信息，等待用户指示
- .shk 文件未生成 → 上报，检查 NeedsShaderStableKeys 配置

**重要提示**：此步骤可能需要 10-60 分钟，取决于项目资源规模。

---

## Step 3：收集 PSO 记录（.rec.upipelinecache）

**需要的 config.json 字段**：`package.output_dir`、`project.project_name`、`rec_source_rel`

**操作**：
1. 检查打包输出目录下是否有可执行文件：
   `{package_output_dir}/{project_name}.exe` 或
   `{package_output_dir}/Windows/{project_name}/{project_name}.exe`

2. **提示用户手动操作**：
   ```
   请手动运行打包后的程序，并执行以下操作以收集 PSO 记录：
   - 遍历游戏中所有关卡/场景
   - 触发各种特效（粒子、后处理等）
   - 切换不同画质设置
   - 打开各种 UI 界面
   - 正常游玩一段时间（覆盖尽可能多的渲染状态组合）

   完成后退出游戏。程序会自动将 PSO 记录保存到 .rec.upipelinecache 文件。
   ```

3. 用户确认已运行后，检查 PSO 记录文件：
   在 `{package_output_dir}/{project_name}/Saved/CollectedPSOs/` 下查找 `*.rec.upipelinecache` 文件。

**验证检查点**：
- `CollectedPSOs` 目录是否存在
- 是否有 `.rec.upipelinecache` 文件

**失败处理**：
- 打包程序 .exe 不存在 → 上报，Step 2 可能失败，等待指示
- `CollectedPSOs` 目录不存在 → 说明程序未正确记录 PSO（检查 `r.ShaderPipelineCache.LogPSO=1` 配置），上报等待指示
- 无 .rec 文件 → 可能程序未渲染足够场景，上报等待指示

---

## Step 4：确认 .shk 稳定键文件可用

**需要的 config.json 字段**：`project.project_dir`、`project.shk_source_relative`

**操作**：
检查以下目录下是否存在 `.shk` 文件：
- `{project_dir}/{shk_source_relative}/`

列出找到的所有 .shk 文件及其大小。

**验证检查点**：
- 目录是否存在
- 是否至少有项目级别的 .shk 文件（名称包含 `{project_name}`）
- .shk 文件大小是否合理（非空文件）

**当前项目已知状态**（作为参考）：
- SM5 项目 .shk：`ShaderStableInfo-{project_name}-PCD3D_SM5.shk`
- SM5 全局 .shk：`ShaderStableInfo-Global-PCD3D_SM5.shk`
- SM6 项目 .shk：`ShaderStableInfo-{project_name}-PCD3D_SM6.shk`
- SM6 全局 .shk：`ShaderStableInfo-Global-PCD3D_SM6.shk`

**失败处理**：
- 目录不存在 → 上报，可能需要重新执行 Cook（Step 2）
- 无 .shk 文件 → 上报，可能需要重新 Cook 或检查配置
- 只有部分 Shader 格式的 .shk → 上报，确认为目标平台的限制

---

## Step 5：汇聚文件到 PSO Cache 工作目录

**需要的 config.json 字段**：
- `pso.cache_work_dir`
- `package.output_dir` / `project.project_dir`
- `pso.rec_source_relative`
- `pso.shk_source_relative`
- `project.project_name`

**操作**：
1. 确保 PSO 缓存工作目录存在：
   如果 `{cache_work_dir}` 不存在，则创建该目录。

2. 复制 `.rec.upipelinecache` 文件：
   源：`{package_output_dir}/{project_name}/Saved/CollectedPSOs/*.rec.upipelinecache`
   目标：`{cache_work_dir}/`

3. 复制 `.shk` 文件：
   源：`{project_dir}/Saved/Cooked/Windows/{project_name}/Metadata/PipelineCaches/*.shk`
   目标：`{cache_work_dir}/`

4. 确认复制结果：列出 `{cache_work_dir}/` 下所有文件。

**验证检查点**：
- 缓存工作目录已创建
- 至少有一个 .rec.upipelinecache 文件
- 至少有一个 .shk 文件

**失败处理**：
- 源文件不存在 → 上报缺失的具体文件路径，等待指示（勿自行创建文件）
- 复制失败 → 上报复制错误，等待指示

---

## Step 6：转换缓存（ShaderPipelineCacheTools expand）

**需要的 config.json 字段**：
- `ue5.editor_cmd_path`
- `project.uproject_file`
- `pso.cache_work_dir`
- `project.project_name`

**操作**：
执行 `ShaderPipelineCacheTools expand` 命令：

```
"{editor_cmd}" "{uproject_file}" `
  -run=ShaderPipelineCacheTools expand `
  "{cache_work_dir}\*.rec.upipelinecache" `
  "{cache_work_dir}\*.shk" `
  "{cache_work_dir}\{project_name}_{shader_format}.spc"
```

对于 `{shader_format}`，需要逐个执行（因为 .rec 和 .shk 可能对应多个 Shader 格式如 SM5/SM6），
如果无法区分格式，可以使用通配输出名：
```
"{cache_work_dir}\{project_name}.spc"
```

**命令说明**：
- `-run=ShaderPipelineCacheTools expand`：调用 UE5 内置的 PSO 缓存工具
- 第一个参数：输入的 .rec.upipelinecache 文件（通配符）
- 第二个参数：输入的 .shk 稳定键文件（通配符）
- 第三个参数：输出的 .spc 稳定 PSO 缓存文件路径

**验证检查点**：
- 命令是否成功执行（exitCode=0）
- 是否有 .spc 文件生成
- .spc 文件大小是否合理（通常几百 KB 到几 MB）

**失败处理**：
- 命令执行失败 → 上报完整输出日志，等待用户指示
- .spc 文件未生成 → 可能是 .rec 或 .shk 文件不匹配，上报等待指示
- editor_cmd 路径不存在 → 上报路径，等待用户修正 config.json

**重要提示**：此步骤可能耗时数分钟，取决于 .rec 文件大小。

---

## Step 7：集成 .spc 文件到项目 Build 目录

**需要的 config.json 字段**：
- `project.project_dir`
- `pso.spc_target_relative`
- `pso.cache_work_dir`
- `project.project_name`

**操作**：
1. 确保目标目录存在：
   如果 `{project_dir}/{spc_target_relative}` 不存在，则创建该目录。

2. 复制 .spc 文件：
   源：`{cache_work_dir}/*.spc`
   目标：`{project_dir}/{spc_target_relative}/`

   目标文件命名通常为 `{project_name}_PCD3D_{shader_format}.spc`，
   例如 `GameFrameworkDev_PCD3D_SM6.spc`

3. 确认目标目录内容。

**验证检查点**：
- `Build/Windows/PipelineCaches/` 目录已创建
- 至少有一个 .spc 文件

**失败处理**：
- .spc 源文件不存在 → Step 6 可能未正确生成，上报等待指示
- 目录创建失败 → 上报错误信息，等待指示
- 复制失败 → 上报错误，等待指示

---

## Step 8：最终打包（包含内建 PSO 缓存）

**需要的 config.json 字段**：
- `ue5.uat_bat_path`
- `project.uproject_file`
- `project.project_name`
- `project.target_platform`
- `package.output_dir`

**操作**：
再次执行 BuildCookRun 打包。引擎会自动检测 `Build/Windows/PipelineCaches/` 下的 .spc 文件，
并将其转换为 `.stable.upipelinecache` 打入最终的包体中：

```
"{uat_bat}" BuildCookRun `
  -project="{uproject_file}" `
  -platform={target_platform} `
  -clientconfig=Development `
  -build -cook -stage -pak -archive `
  -archivedirectory="{package_output_dir}"
```

**验证检查点**：
- UAT 命令成功执行
- 打包产物中应包含 `.stable.upipelinecache` 文件
- 可以在打包输出目录中搜索 `*.stable.upipelinecache` 确认

**失败处理**：
- 打包失败 → 上报 UAT 输出日志中的错误信息，等待指示
- .stable.upipelinecache 未生成 → 可能 .spc 文件未被正确识别，上报等待指示

**注意**：如果之前已有打包产物，可以选择覆盖（添加 `-clean` 参数做干净打包），
或者先行备份之前的打包输出。

---

## 完整流程总结

```
┌─ Step 0: 加载 config.json ──────────────────────────────┐
│  读取所有路径，验证关键路径存在                           │
└──────────────────────────────────────────────────────────┘
    ↓
┌─ Step 1: 验证 PSO 配置 ─────────────────────────────────┐
│  检查 DefaultEngine.ini / DefaultGame.ini 中的 PSO 配置   │
└──────────────────────────────────────────────────────────┘
    ↓
┌─ Step 2: 首次打包 ──────────────────────────────────────┐
│  UAT BuildCookRun → 生成 .shk 稳定键文件                  │
└──────────────────────────────────────────────────────────┘
    ↓
┌─ Step 3: 收集 PSO ──────────────────────────────────────┐
│  用户运行打包程序遍历场景 → 生成 .rec.upipelinecache       │
└──────────────────────────────────────────────────────────┘
    ↓
┌─ Step 4: 确认 .shk ─────────────────────────────────────┐
│  检查 Cook 输出的 .shk 稳定键文件                         │
└──────────────────────────────────────────────────────────┘
    ↓
┌─ Step 5: 汇聚文件 ──────────────────────────────────────┐
│  复制 .rec + .shk → PSO Cache 工作目录                    │
└──────────────────────────────────────────────────────────┘
    ↓
┌─ Step 6: 转换缓存 ──────────────────────────────────────┐
│  ShaderPipelineCacheTools expand → 生成 .spc              │
└──────────────────────────────────────────────────────────┘
    ↓
┌─ Step 7: 集成 .spc ─────────────────────────────────────┐
│  复制 .spc → Build/Windows/PipelineCaches/                │
└──────────────────────────────────────────────────────────┘
    ↓
┌─ Step 8: 最终打包 ──────────────────────────────────────┐
│  再次 BuildCookRun → 内建 PSO 缓存打入包体                │
└──────────────────────────────────────────────────────────┘
```

---

## 关键文件关系图

```
项目源目录 (E:\UE_Project\UE5GameFramework\GameFrameworkDev)
├── Config\
│   ├── DefaultEngine.ini     ← [Step 1] PSO 配置检查
│   └── DefaultGame.ini       ← [Step 1] 材质库共享配置检查
├── Saved\
│   └── Cooked\Windows\GameFrameworkDev\Metadata\PipelineCaches\
│       ├── *.shk              ← [Step 4] 引擎 Cook 自动生成
│       └── ...                ← [Step 5] 复制到 PSOCache 工作目录
├── Build\
│   └── Windows\PipelineCaches\
│       └── *.spc              ← [Step 7] 集成到此目录
└── PSOCache\                  ← [Step 5] 手动创建的临时工作目录
    ├── *.rec.upipelinecache   ← [Step 5] 从打包程序 Saved/CollectedPSOs/ 复制
    ├── *.shk                  ← [Step 5] 从项目 Cook 输出复制
    └── *.spc                  ← [Step 6] expand 命令输出

打包输出目录 (E:\UE_Project\UE5GameFramework\GameFrameworkDev\Package)
├── GameFrameworkDev.exe       ← [Step 2/8] 打包产物
└── GameFrameworkDev\
    └── Saved\CollectedPSOs\
        └── *.rec.upipelinecache  ← [Step 3] 打包程序运行时自动生成
```

---

## 注意事项

1. **必须从 Step 0 开始**：每一步都依赖 config.json 中的路径，切勿跳过。
2. **每步必须验证**：执行完操作后立即检查结果，不要假设操作成功。
3. **问题必须上报**：任何异常情况必须先汇报给用户，等待明确指示后再继续。
4. **路径安全**：只有 `PSOCache` 和 `Package` 目录可在不存在时自动创建（config.json 已预设），
   其他目录如不存在必须上报。
5. **Shader 格式**：当前项目配置了 SM5 和 SM6，.spc 文件和 .shk 文件是按格式独立的。
   Step 6 的 expand 命令可能需要针对每个 Shader 格式分别执行。
6. **打包程序 Saved 目录**：注意区分"项目源目录的 Saved"和"打包程序运行时的 Saved"。
   .rec 文件在打包程序运行后的 Saved 目录下，非项目源目录。
