r"""
ConfigManager - 全局配置管理器
负责 config.json 的加载、保存、验证
数据存储在 %LOCALAPPDATA%\UE5PSOPackager\config.json
"""

import os
import re
import json
import shutil
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


# ---- 数据类 ----

@dataclass
class UE5VersionConfig:
    """UE5 引擎版本配置（只需配置 editor_cmd_path，其他自动推导）"""
    install_dir: str = ""
    editor_cmd_path: str = ""
    uat_bat_path: str = ""

    def resolve_from_editor_cmd(self):
        """根据 editor_cmd_path 自动推导 install_dir 和 uat_bat_path

        editor_cmd_path 典型路径: {install_dir}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe
        uat_bat_path:             {install_dir}/Engine/Build/BatchFiles/RunUAT.bat
        """
        if not self.editor_cmd_path:
            return

        cmd = Path(self.editor_cmd_path)
        if not cmd.exists():
            return

        # UnrealEditor-Cmd.exe 位于 Engine/Binaries/Win64/，往上 3 层到 Engine，再往上一层即 install_dir
        engine_dir = cmd.parent.parent.parent  # Win64 -> Binaries -> Engine
        self.install_dir = str(engine_dir.parent)  # Engine 的父目录即 install_dir
        self.uat_bat_path = str(engine_dir / "Build" / "BatchFiles" / "RunUAT.bat")

    def validate(self) -> list[str]:
        """验证路径是否存在，返回错误列表"""
        errors = []
        if not self.editor_cmd_path:
            errors.append("未配置 UnrealEditor-Cmd.exe 路径")
        elif not Path(self.editor_cmd_path).exists():
            errors.append(f"UnrealEditor-Cmd.exe 路径不存在: {self.editor_cmd_path}")
        if self.uat_bat_path and not Path(self.uat_bat_path).exists():
            errors.append(f"RunUAT.bat 路径不存在: {self.uat_bat_path}")
        return errors


@dataclass
class ProjectConfig:
    """单个项目配置"""
    name: str = ""
    ue5_version: str = ""
    project_dir: str = ""
    uproject_file: str = ""
    target_platform: str = "Win64"
    output_dir: str = ""
    pso_cache_work_dir: str = ""
    shk_source_relative: str = "Saved\\Cooked\\Windows\\{project_name}\\Metadata\\PipelineCaches"
    rec_source_relative: str = "Saved\\CollectedPSOs"
    spc_target_relative: str = "Build\\Windows\\PipelineCaches"

    # ---- Step 9: 测试 PSO 覆盖范围 ---- 
    step9_auto_close_minutes: int = 60        # 自动关闭时间（分钟），范围 60-14400
    step9_logpso: bool = True                 # 是否启用 -logpso 参数
    step9_clear_driver_cache: bool = True     # 是否启用 -clearPSODriverCache 清除驱动缓存

    # ---- PSO 收集地图 ----
    pso_collect_map: str = ""                 # PSO收集时启动的关卡名（如 /PSOCacheSystem/Maps/PSOCoverageMap）

    def validate(self) -> list[str]:
        """验证项目配置，返回错误列表"""
        errors = []
        if not self.name:
            errors.append("项目名称为空")
        if not self.ue5_version:
            errors.append("未关联 UE 版本")
        if not self.uproject_file:
            errors.append("未指定 .uproject 文件")
        elif not Path(self.uproject_file).exists():
            errors.append(f".uproject 文件不存在: {self.uproject_file}")
        if not self.output_dir:
            errors.append("未指定打包输出目录")
        if not self.pso_cache_work_dir:
            errors.append("未指定 PSO 缓存工作目录")
        return errors

    def get_uproject_name(self) -> str:
        """获取 UE 打包时的可执行文件名称（exe 文件名）
        
        UE 打包的 exe 名称由 Build Target 决定，而非 .uproject 的 Modules。
        Target 名称来自 Source/*.Target.cs 文件名。
        
        优先级：
        1. .uproject 顶层的 "Name" 字段
        2. Source/ 目录下的 Game Target 名称（不含 Editor/Server/Client 后缀的 .Target.cs）
        3. .uproject 文件名（去掉 .uproject 后缀）
        4. 降级到 self.name
        
        **注意**：此方法返回的是 EXE 二进制文件名，不是打包目录中的文件夹名。
        打包目录 `Windows/` 下的文件夹名需使用 `get_uproject_stem()`。
        
        读取失败时降级返回 self.name。
        """
        if not self.uproject_file:
            return self.name
        try:
            import glob
            uproject_path = Path(self.uproject_file)
            if not uproject_path.exists():
                return self.name
            with open(uproject_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            
            # 1. 顶层 Name 字段
            name = data.get("Name")
            if name:
                return str(name)
            
            # 2. Source/ 目录下的 Build Target：不含 Editor/Server/Client 后缀的 .Target.cs
            source_dir = uproject_path.parent / "Source"
            if source_dir.is_dir():
                target_files = list(source_dir.glob("*.Target.cs"))
                # 排除 Editor / Server / Client target，优先纯游戏 target
                game_targets = []
                for tf in target_files:
                    stem = tf.stem.replace(".Target", "")  # LyraGame.Target → LyraGame
                    if stem.endswith("Editor") or stem.endswith("Server") or stem.endswith("Client"):
                        continue
                    game_targets.append(stem)
                if game_targets:
                    return game_targets[0]
            
            # 3. .uproject 文件名（去掉 .uproject 后缀）
            stem = uproject_path.stem
            if stem:
                return stem
            
            return self.name
        except (json.JSONDecodeError, OSError):
            return self.name

    def get_uproject_stem(self) -> str:
        """获取 .uproject 文件名（不含扩展名）
        
        这是打包输出目录中 `Windows/<name>/` 的文件夹名，
        以及 `Saved/Cooked/Windows/<name>/` 的文件夹名。
        
        UE 的 UAT -archive 使用 .uproject 文件名作为 Stage 目录名，
        与 .uproject 内部 JSON 的 Name 字段可能不同。
        
        示例：
        - uproject 文件：LyraStarterGame56.uproject → 返回 "LyraStarterGame56"
        - uproject JSON Name 字段："LyraGame"（不影响此方法）
        """
        if not self.uproject_file:
            return self.name
        try:
            uproject_path = Path(self.uproject_file)
            if not uproject_path.exists():
                return self.name
            return uproject_path.stem
        except (OSError, ValueError):
            return self.name

    def resolve_shk_source_dir(self) -> str:
        """解析 .shk 源目录（替换 {project_name} 占位符）
        
        .shk 文件由 Cook 阶段生成，位于项目源码目录下：
        {project_dir}/Saved/Cooked/Windows/{uproject_stem}/Metadata/PipelineCaches
        
        文件夹名使用 .uproject 文件名（非内部 Name 字段）。
        """
        base = self.project_dir
        rel = self.shk_source_relative.replace("{project_name}", self.get_uproject_stem())
        return str(Path(base) / rel)

    def resolve_rec_source_dir(self) -> str:
        """解析 .rec 源目录（打包游戏运行时收集的 PSO 记录）
        
        .rec.upipelinecache 文件由打包游戏运行时产生，位于输出目录下：
        output_dir/Windows/{uproject_stem}/Saved/CollectedPSOs
        """
        base = Path(self.output_dir) / "Windows" / self.get_uproject_stem()
        return str(base / self.rec_source_relative)

    def resolve_packaged_rec_source_dir(self) -> str:
        """解析打包后游戏的 .rec 源目录
        
        打包游戏位于 output_dir/Windows/{uproject_stem}/，其 Saved 目录也在该位置下。
        文件夹名使用 .uproject 文件名（非内部 Name 字段）。
        """
        return str(Path(self.output_dir) / "Windows" / self.get_uproject_stem() / self.rec_source_relative)

    def resolve_spc_target_dir(self) -> str:
        """解析 .spc 目标目录"""
        return str(Path(self.project_dir) / self.spc_target_relative)


def read_shader_formats_from_ini(project_dir: str | None) -> str:
    """从 DefaultEngine.ini 解析 Shader 格式，返回 UAT 命令行格式字符串

    示例："PCD3D_SM5+PCD3D_SM6"（供 -ShaderFormats= 使用）
    始终根据实际 INI 配置动态读取，不依赖任何持久化存储。
    """
    if not project_dir:
        return "PCD3D_SM6"
    ini_path = Path(project_dir) / "Config" / "DefaultEngine.ini"
    if not ini_path.exists():
        return "PCD3D_SM6"
    try:
        content = ini_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return "PCD3D_SM6"

    m_section = re.search(
        r'\[/Script/WindowsTargetPlatform\.WindowsTargetSettings\](.*?)(?=^\s*\[|\Z)',
        content, re.DOTALL | re.MULTILINE | re.IGNORECASE
    )
    if not m_section:
        return "PCD3D_SM6"
    section = m_section.group(1)

    formats = set()
    for m in re.finditer(r'^\+.*TargetedShaderFormats\s*=\s*(\S+)', section, re.MULTILINE):
        formats.add(m.group(1).strip())
    if not formats:
        return "PCD3D_SM6"
    return "+".join(sorted(formats, key=_shader_format_priority_sort_key))


def _shader_format_priority_sort_key(fmt: str) -> tuple:
    """Shader 格式排序键：SM6 最先，SM5 其次，其余按字母序。

    用于 read_shader_formats_list_from_ini 等函数的排序，
    确保 UI 下拉列表按 用户预期的优先级 排列。
    """
    upper = fmt.upper()
    if "SM6" in upper:
        return (0, fmt)
    if "SM5" in upper:
        return (1, fmt)
    return (2, fmt)


def get_default_shader_format(project_dir: str | None) -> str:
    """获取推荐的默认 Shader 格式：SM6 > SM5 > 列表首项 > PCD3D_SM6

    供 UI 参数填充时作为 ShaderFormat 下拉框的默认选中值。
    """
    sf_list = read_shader_formats_list_from_ini(project_dir)
    if not sf_list:
        return "PCD3D_SM6"
    return sf_list[0]  # 列表已按优先级排序，首项即推荐


def read_shader_formats_list_from_ini(project_dir: str | None) -> list[str]:
    """从 DefaultEngine.ini 解析 Shader 格式，返回实际生效的格式列表

    仅解析 + 前缀的行（添加项），- 前缀的行（删除项）不纳入。
    排序规则：SM6 优先 → SM5 优先 → 其余按字母序。

    示例：["PCD3D_SM6", "PCD3D_SM5"]
    """
    if not project_dir:
        return ["PCD3D_SM6"]
    ini_path = Path(project_dir) / "Config" / "DefaultEngine.ini"
    if not ini_path.exists():
        return ["PCD3D_SM6"]
    try:
        content = ini_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ["PCD3D_SM6"]

    m_section = re.search(
        r'\[/Script/WindowsTargetPlatform\.WindowsTargetSettings\](.*?)(?=^\s*\[|\Z)',
        content, re.DOTALL | re.MULTILINE | re.IGNORECASE
    )
    if not m_section:
        return ["PCD3D_SM6"]
    section = m_section.group(1)

    formats = set()
    for m in re.finditer(r'^\+.*TargetedShaderFormats\s*=\s*(\S+)', section, re.MULTILINE):
        formats.add(m.group(1).strip())
    if not formats:
        return ["PCD3D_SM6"]
    return sorted(formats, key=_shader_format_priority_sort_key)


def read_pso_coverage_map_from_ini(project_dir: str | None) -> str:
    """从 DefaultGame.ini 读取 PSOCoverageMap 配置的关卡路径

    返回关卡路径（如 /PSOCacheSystem/Maps/PSOCoverageMap），未配置则返回空字符串。
    """
    if not project_dir:
        return ""
    ini_path = Path(project_dir) / "Config" / "DefaultGame.ini"
    if not ini_path.exists():
        return ""
    try:
        content = ini_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""

    m_section = re.search(
        r'\[/Script/PSOCacheSystem\.PSOCacheSettings\](.*?)(?=^\s*\[|\Z)',
        content, re.DOTALL | re.MULTILINE | re.IGNORECASE
    )
    if not m_section:
        return ""
    section = m_section.group(1)

    m = re.search(r'^\s*PSOCoverageMap\s*=\s*(\S+)', section, re.MULTILINE)
    if not m:
        return ""
    return m.group(1).strip()


# ---- ConfigManager ----

class ConfigManager:
    """全局配置管理器，负责 config.json 的加载、保存、验证"""

    APP_NAME = "UE5PSOPackager"
    CONFIG_FILENAME = "config.json"

    def __init__(self):
        self.ue5_versions: dict[str, UE5VersionConfig] = {}
        self.projects: list[ProjectConfig] = []

        # 配置存储路径：%LOCALAPPDATA%\UE5PSOPackager\config.json
        self._config_dir = Path(os.environ.get("LOCALAPPDATA", "")) / self.APP_NAME
        self._config_path = self._config_dir / self.CONFIG_FILENAME

        # 默认配置模板路径（本地开发用）
        self._default_config_path = Path(__file__).parent / "resources" / "default_config.json"

    # ---- 加载 / 保存 ----

    def load(self) -> bool:
        """
        加载配置，若配置文件不存在则尝试从模板复制
        返回 True 表示加载成功
        """
        # 可能存在旧格式（单项目），尝试转换
        raw = None

        if self._config_path.exists():
            try:
                with open(self._config_path, "r", encoding="utf-8") as f:
                    raw = json.load(f)
            except (json.JSONDecodeError, OSError) as e:
                # 配置损坏，尝试从模板恢复
                raw = None

        # 首次运行或配置损坏 —— 从模板复制
        if raw is None:
            if self._default_config_path.exists():
                self._config_dir.mkdir(parents=True, exist_ok=True)
                shutil.copy(self._default_config_path, self._config_path)
                with open(self._config_path, "r", encoding="utf-8") as f:
                    raw = json.load(f)
            else:
                raw = {"ue5_versions": {}, "projects": []}

        return self._deserialize(raw)

    def save(self) -> bool:
        """保存配置到文件"""
        try:
            self._config_dir.mkdir(parents=True, exist_ok=True)
            with open(self._config_path, "w", encoding="utf-8") as f:
                json.dump(self._serialize(), f, indent=2, ensure_ascii=False)
            return True
        except OSError:
            return False

    # ---- 序列化 / 反序列化 ----

    def _serialize(self) -> dict:
        """将当前配置序列化为字典"""
        ue5_dict = {}
        for ver, cfg in self.ue5_versions.items():
            ue5_dict[ver] = {
                "install_dir": cfg.install_dir,
                "editor_cmd_path": cfg.editor_cmd_path,
                "uat_bat_path": cfg.uat_bat_path,
            }

        projects_list = []
        for proj in self.projects:
            projects_list.append({
                "name": proj.name,
                "ue5_version": proj.ue5_version,
                "project_dir": proj.project_dir,
                "uproject_file": proj.uproject_file,
                "target_platform": proj.target_platform,
                "output_dir": proj.output_dir,
                "pso_cache_work_dir": proj.pso_cache_work_dir,
                "shk_source_relative": proj.shk_source_relative,
                "rec_source_relative": proj.rec_source_relative,
                "spc_target_relative": proj.spc_target_relative,
                "step9_auto_close_minutes": proj.step9_auto_close_minutes,
                "step9_logpso": proj.step9_logpso,
                "step9_clear_driver_cache": proj.step9_clear_driver_cache,
                "pso_collect_map": proj.pso_collect_map,
            })

        return {
            "ue5_versions": ue5_dict,
            "projects": projects_list,
        }

    def _deserialize(self, raw: dict) -> bool:
        """从字典反序列化配置"""
        self.ue5_versions.clear()
        self.projects.clear()

        # 解析 UE 版本
        ue5_dict = raw.get("ue5_versions", {})
        if isinstance(ue5_dict, dict):
            for ver, data in ue5_dict.items():
                ue5 = UE5VersionConfig()
                if isinstance(data, dict):
                    ue5.install_dir = data.get("install_dir", "")
                    ue5.editor_cmd_path = data.get("editor_cmd_path", "")
                    ue5.uat_bat_path = data.get("uat_bat_path", "")
                # 自动推导缺失的路径（用户只需配置 editor_cmd_path）
                if ue5.editor_cmd_path:
                    ue5.resolve_from_editor_cmd()
                self.ue5_versions[ver] = ue5

        # 解析项目列表
        projects_list = raw.get("projects", [])
        if isinstance(projects_list, list):
            for data in projects_list:
                proj = ProjectConfig()
                if isinstance(data, dict):
                    proj.name = data.get("name", "")
                    proj.ue5_version = data.get("ue5_version", "")
                    proj.project_dir = data.get("project_dir", "")
                    proj.uproject_file = data.get("uproject_file", "")
                    proj.target_platform = data.get("target_platform", "Win64")
                    proj.output_dir = data.get("output_dir", "")
                    proj.pso_cache_work_dir = data.get("pso_cache_work_dir", "")
                    proj.shk_source_relative = data.get("shk_source_relative",
                        "Saved\\Cooked\\Windows\\{project_name}\\Metadata\\PipelineCaches")
                    proj.rec_source_relative = data.get("rec_source_relative", "Saved\\CollectedPSOs")
                    proj.spc_target_relative = data.get("spc_target_relative", "Build\\Windows\\PipelineCaches")
                    proj.step9_auto_close_minutes = data.get("step9_auto_close_minutes", 60)
                    proj.step9_logpso = data.get("step9_logpso", True)
                    proj.step9_clear_driver_cache = data.get("step9_clear_driver_cache", True)
                    proj.pso_collect_map = data.get("pso_collect_map", "")
                self.projects.append(proj)

        return True

    # ---- 查询 ----

    def get_project(self, index: int) -> Optional[ProjectConfig]:
        """按索引获取项目配置"""
        if 0 <= index < len(self.projects):
            return self.projects[index]
        return None

    def get_ue5_version(self, version_key: str) -> Optional[UE5VersionConfig]:
        """按版本号获取 UE 配置"""
        return self.ue5_versions.get(version_key)

    def get_project_ue5(self, project: ProjectConfig) -> Optional[UE5VersionConfig]:
        """获取项目关联的 UE 版本配置"""
        return self.get_ue5_version(project.ue5_version)

    # ---- 增删 ----

    def add_ue5_version(self, version_key: str, config: UE5VersionConfig):
        """添加或更新 UE 版本配置"""
        self.ue5_versions[version_key] = config

    def remove_ue5_version(self, version_key: str) -> bool:
        """删除 UE 版本配置（如有项目引用则拒绝）"""
        for proj in self.projects:
            if proj.ue5_version == version_key:
                return False  # 有项目引用，禁止删除
        if version_key in self.ue5_versions:
            del self.ue5_versions[version_key]
            return True
        return False

    def add_project(self, project: ProjectConfig) -> int:
        """添加项目，返回新项目索引"""
        self.projects.append(project)
        return len(self.projects) - 1

    def remove_project(self, index: int) -> bool:
        """按索引删除项目"""
        if 0 <= index < len(self.projects):
            del self.projects[index]
            return True
        return False

    # ---- 验证 ----

    def validate_all(self) -> list[str]:
        """验证所有 UE 版本和项目配置，返回错误列表"""
        errors = []

        # 验证 UE 版本
        for ver, ue5 in self.ue5_versions.items():
            for err in ue5.validate():
                errors.append(f"[UE {ver}] {err}")

        # 验证项目
        for idx, proj in enumerate(self.projects):
            for err in proj.validate():
                errors.append(f"[项目 {idx}:{proj.name}] {err}")

            # 检查关联的 UE 版本是否存在
            if proj.ue5_version and proj.ue5_version not in self.ue5_versions:
                errors.append(f"[项目 {idx}:{proj.name}] 关联的 UE 版本 '{proj.ue5_version}' 未配置")

        return errors
