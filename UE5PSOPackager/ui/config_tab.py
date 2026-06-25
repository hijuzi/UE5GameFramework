"""
ConfigTab - 配置管理标签页
UE 版本管理 + 项目配置管理，支持多 UE 版本和多项目切换
"""
import json
import re
from collections import namedtuple

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QListWidget, QListWidgetItem, QFormLayout,
    QLineEdit, QPushButton, QMessageBox, QLabel,
    QComboBox, QSplitter, QFileDialog, QDialog,
    QDialogButtonBox, QTextEdit, QScrollArea,
    QCheckBox, QPlainTextEdit, QSizePolicy, QGridLayout, QFrame,
)
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QFont
from pathlib import Path

from config_manager import ConfigManager, UE5VersionConfig, ProjectConfig
from step_runner import PSO_REQUIRED_CONFIGS

# ---- 命令行参数行定义 ----
ParamRow = namedtuple("ParamRow", ["desc", "flag", "value_key", "widget_type", "choices"])
# widget_type: "path_file", "path_dir", "text", "check", "readonly", "combo"
# choices: 仅 combo 有效，列表 [(显示文本, 值), ...]

_FIRST_FINAL_PARAMS = [
    ParamRow("项目路径", "-project", "project", "readonly", None),
    ParamRow("目标平台", "-platform", "platform", "combo",
             [("Win64", "Win64")]),
    ParamRow("构建配置", "-clientconfig", "config", "combo",
             [("开发 (Development)", "Development"),
              ("发布 (Shipping)", "Shipping"),
              ("测试 (Test)", "Test")]),
    ParamRow("输出目录", "-archivedirectory", "output_dir", "readonly", None),
    ParamRow("Shader格式", "-ShaderFormats", "shader_formats", "readonly", None),
    ParamRow("引擎EXE", "-unrealexe", "unreal_exe", "readonly", None),
    ParamRow("编译", "-compile", "compile", "check", None),
    ParamRow("构建", "-build", "build", "check", None),
    ParamRow("烘焙", "-cook", "cook", "check", None),
    ParamRow("部署", "-stage", "stage", "check", None),
    ParamRow("PAK打包", "-pak", "pak", "check", None),
    ParamRow("归档", "-archive", "archive", "check", None),
    ParamRow("Crash上报", "-CrashReporter", "crash_reporter", "check", None),
    ParamRow("UTF8输出", "-utf8output", "utf8_output", "check", None),
]

_CACHE_CONVERT_PARAMS = [
    ParamRow("项目路径", "-project", "project", "readonly", None),
    ParamRow("运行命令", "-run=", "run_cmd", "readonly", None),
    ParamRow("REC源文件", "", "rec_source", "readonly", None),
    ParamRow("SHK源文件", "", "shk_source", "readonly", None),
    ParamRow("目标SPC", "", "spc_target", "readonly", None),
    ParamRow("NullRHI", "-NullRHI", "null_rhi", "check", None),
    ParamRow("无交互", "-unattended", "unattended", "check", None),
]


def _read_engine_association(uproject_path: Path) -> str | None:
    """从 .uproject 文件读取 EngineAssociation，失败返回 None"""
    try:
        if not uproject_path.exists() or uproject_path.suffix.lower() != ".uproject":
            return None
        with open(uproject_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data.get("EngineAssociation", "")
    except (json.JSONDecodeError, OSError):
        return None


def _read_shader_formats_from_ini(project_dir: str | None) -> str:
    """从 DefaultEngine.ini 的 WindowsTargetSettings 段解析 Shader 格式

    解析 [WindowsTargetPlatform.WindowsTargetSettings] 下的：
        +D3D12TargetedShaderFormats=PCD3D_SM6
        +D3D11TargetedShaderFormats=PCD3D_SM6
    返回 "PCD3D_SM6+PCD3D_SM5" 格式字符串（供 -ShaderFormats 使用）
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

    # 定位 [WindowsTargetPlatform.WindowsTargetSettings] 段
    m_section = re.search(
        r'\[/Script/WindowsTargetPlatform\.WindowsTargetSettings\](.*?)(?=\[|$)',
        content, re.DOTALL | re.IGNORECASE
    )
    if not m_section:
        return "PCD3D_SM6"
    section = m_section.group(1)

    # 提取所有 +TargetedShaderFormats 配置行
    formats = set()
    for m in re.finditer(r'^\+.*TargetedShaderFormats\s*=\s*(\S+)', section, re.MULTILINE):
        formats.add(m.group(1).strip())
    if not formats:
        return "PCD3D_SM6"
    return "+".join(sorted(formats))


class ConfigTab(QWidget):
    """配置管理标签页"""

    config_changed = Signal()

    def __init__(self, config: ConfigManager, parent=None):
        super().__init__(parent)
        self._config = config
        self._current_ue5_version: str = ""
        self._current_project_index: int = -1

        # 中间面板控件
        self._mid_engine_labels: dict[str, QLabel] = {}
        self._mid_game_labels: dict[str, QLabel] = {}
        self._mid_pso_checks: list[QCheckBox] = []
        # 参数表格行：{value_key: (desc_label, value_widget, browse_btn or None)}
        self._mid_first_build_rows: dict[str, tuple] = {}
        self._mid_cache_convert_rows: dict[str, tuple] = {}
        self._mid_final_build_rows: dict[str, tuple] = {}
        self._mid_scroll: QScrollArea = None

        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(10)

        # 顶部：保存按钮
        top_bar = QHBoxLayout()
        top_bar.addStretch()
        btn_save = QPushButton("💾  保存配置")
        btn_save.setObjectName("btnPrimary")
        btn_save.clicked.connect(self._on_save)
        top_bar.addWidget(btn_save)
        layout.addLayout(top_bar)

        splitter = QSplitter(Qt.Orientation.Horizontal)

        # ---- 左侧面板 ----
        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)
        left_layout.setContentsMargins(0, 0, 0, 0)

        ue_group = QGroupBox("UE 引擎版本")
        ue_layout = QVBoxLayout(ue_group)
        self._ue_list = QListWidget()
        self._ue_list.currentTextChanged.connect(self._on_ue_selected)
        ue_layout.addWidget(self._ue_list)

        ue_btn_row = QHBoxLayout()
        btn_add_ue = QPushButton("+ 新增")
        btn_add_ue.clicked.connect(self._on_add_ue)
        ue_btn_row.addWidget(btn_add_ue)
        btn_del_ue = QPushButton("- 删除")
        btn_del_ue.clicked.connect(self._on_del_ue)
        ue_btn_row.addWidget(btn_del_ue)
        ue_layout.addLayout(ue_btn_row)
        left_layout.addWidget(ue_group)

        proj_group = QGroupBox("项目列表")
        proj_layout = QVBoxLayout(proj_group)
        self._proj_list = QListWidget()
        self._proj_list.currentRowChanged.connect(self._on_proj_selected)
        proj_layout.addWidget(self._proj_list)

        proj_btn_row = QHBoxLayout()
        btn_add_proj = QPushButton("+ 新增")
        btn_add_proj.clicked.connect(self._on_add_proj)
        proj_btn_row.addWidget(btn_add_proj)
        btn_del_proj = QPushButton("- 删除")
        btn_del_proj.clicked.connect(self._on_del_proj)
        proj_btn_row.addWidget(btn_del_proj)
        proj_layout.addLayout(proj_btn_row)
        left_layout.addWidget(proj_group)

        splitter.addWidget(left_panel)

        # ---- 中间面板 ----
        mid_panel = self._create_middle_panel()
        mid_panel.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        splitter.addWidget(mid_panel)

        # ---- 右侧面板 ----
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_layout.setContentsMargins(8, 0, 0, 0)

        ue_form_group = QGroupBox("UE 引擎详情")
        ue_form_layout = QFormLayout(ue_form_group)
        row_cmd = QHBoxLayout()
        self._ue_editor_cmd_path = QLineEdit()
        self._ue_editor_cmd_path.setPlaceholderText("选择 ...\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe，其他自动推导")
        self._ue_editor_cmd_path.textChanged.connect(self._on_editor_cmd_changed)
        btn_cmd = QPushButton("...")
        btn_cmd.setFixedWidth(32)
        btn_cmd.clicked.connect(lambda: self._browse_file(
            self._ue_editor_cmd_path, "UnrealEditor-Cmd.exe (UnrealEditor-Cmd.exe)"))
        row_cmd.addWidget(self._ue_editor_cmd_path)
        row_cmd.addWidget(btn_cmd)
        ue_form_layout.addRow("Editor-Cmd:", row_cmd)

        self._ue_install_label = QLabel("—")
        self._ue_install_label.setStyleSheet("color: #888888; padding: 6px 8px;")
        ue_form_layout.addRow("→ 引擎目录:", self._ue_install_label)

        self._ue_uat_label = QLabel("—")
        self._ue_uat_label.setStyleSheet("color: #888888; padding: 6px 8px;")
        ue_form_layout.addRow("→ RunUAT.bat:", self._ue_uat_label)

        self._ue_status = QLabel("")
        ue_form_layout.addRow("", self._ue_status)
        right_layout.addWidget(ue_form_group)

        proj_form_group = QGroupBox("项目详情")
        proj_form_layout = QFormLayout(proj_form_group)
        self._proj_name = QLineEdit()
        proj_form_layout.addRow("项目名称:", self._proj_name)

        self._proj_ue_version = QComboBox()
        self._proj_ue_version.setEditable(False)
        proj_form_layout.addRow("关联 UE 版本:", self._proj_ue_version)

        self._proj_ue_status = QLabel("")
        proj_form_layout.addRow("", self._proj_ue_status)

        row_dir = QHBoxLayout()
        self._proj_dir = QLineEdit()
        self._proj_dir.setPlaceholderText("项目根目录")
        btn_proj_dir = QPushButton("...")
        btn_proj_dir.setFixedWidth(32)
        btn_proj_dir.clicked.connect(lambda: self._browse_dir(self._proj_dir))
        row_dir.addWidget(self._proj_dir)
        row_dir.addWidget(btn_proj_dir)
        proj_form_layout.addRow("项目目录:", row_dir)

        row_uproj = QHBoxLayout()
        self._proj_uproject = QLineEdit()
        self._proj_uproject.setPlaceholderText(".uproject 文件路径")
        self._proj_uproject.textChanged.connect(self._on_uproject_changed)
        btn_uproj = QPushButton("...")
        btn_uproj.setFixedWidth(32)
        btn_uproj.clicked.connect(lambda: self._browse_file(self._proj_uproject, "UProject Files (*.uproject)"))
        row_uproj.addWidget(self._proj_uproject)
        row_uproj.addWidget(btn_uproj)
        proj_form_layout.addRow("UProject:", row_uproj)

        row_output = QHBoxLayout()
        self._proj_output = QLineEdit()
        btn_output = QPushButton("...")
        btn_output.setFixedWidth(32)
        btn_output.clicked.connect(lambda: self._browse_dir(self._proj_output))
        row_output.addWidget(self._proj_output)
        row_output.addWidget(btn_output)
        proj_form_layout.addRow("打包输出:", row_output)

        row_pso = QHBoxLayout()
        self._proj_pso_work = QLineEdit()
        btn_pso = QPushButton("...")
        btn_pso.setFixedWidth(32)
        btn_pso.clicked.connect(lambda: self._browse_dir(self._proj_pso_work))
        row_pso.addWidget(self._proj_pso_work)
        row_pso.addWidget(btn_pso)
        proj_form_layout.addRow("PSO 工作目录:", row_pso)

        self._proj_shk_rel = QLineEdit()
        self._proj_shk_rel.setReadOnly(True)
        self._proj_shk_rel.setText("Saved\\Cooked\\Windows\\{project_name}\\Metadata\\PipelineCaches")
        self._proj_shk_rel.setStyleSheet(
            "QLineEdit { background-color: #2A2A2A; color: #888888; border: 1px solid #3D3D3D;"
            " border-radius: 2px; padding: 2px 6px; }"
        )
        proj_form_layout.addRow("SHK 相对路径:", self._proj_shk_rel)

        self._proj_rec_rel = QLineEdit()
        self._proj_rec_rel.setReadOnly(True)
        self._proj_rec_rel.setText("Saved\\CollectedPSOs")
        self._proj_rec_rel.setStyleSheet(
            "QLineEdit { background-color: #2A2A2A; color: #888888; border: 1px solid #3D3D3D;"
            " border-radius: 2px; padding: 2px 6px; }"
        )
        proj_form_layout.addRow("REC 相对路径:", self._proj_rec_rel)

        self._proj_spc_rel = QLineEdit()
        self._proj_spc_rel.setReadOnly(True)
        self._proj_spc_rel.setText("Build\\Windows\\PipelineCaches")
        self._proj_spc_rel.setStyleSheet(
            "QLineEdit { background-color: #2A2A2A; color: #888888; border: 1px solid #3D3D3D;"
            " border-radius: 2px; padding: 2px 6px; }"
        )
        proj_form_layout.addRow("SPC 相对路径:", self._proj_spc_rel)

        self._proj_status = QLabel("")
        proj_form_layout.addRow("", self._proj_status)
        right_layout.addWidget(proj_form_group)

        splitter.addWidget(right_panel)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 2)
        splitter.setStretchFactor(2, 2)

        layout.addWidget(splitter)

        self.refresh()

    # ============================================================
    # 中间面板：6 组配置预览
    # ============================================================

    def _create_middle_panel(self) -> QWidget:
        """创建中间面板"""
        self._mid_scroll = QScrollArea()
        self._mid_scroll.setWidgetResizable(True)
        self._mid_scroll.setStyleSheet("QScrollArea { border: none; background-color: #1E1E1E; }")

        content = QWidget()
        cl = QVBoxLayout(content)
        cl.setContentsMargins(4, 0, 4, 0)
        cl.setSpacing(8)

        # Part 1: Engine 配置
        eg = QGroupBox("Engine 配置")
        el = QFormLayout(eg)
        el.setSpacing(4)
        for key in ("Editor-Cmd", "引擎目录", "RunUAT.bat", "Shader 格式"):
            lbl = QLabel("—")
            lbl.setWordWrap(True)
            lbl.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
            lbl.setStyleSheet("color: #AAAAAA; padding: 2px 6px; font-size: 12px;")
            el.addRow(f"{key}:", lbl)
            self._mid_engine_labels[key] = lbl
        cl.addWidget(eg)

        # Part 2: Game 配置
        gg = QGroupBox("Game 配置")
        gl = QFormLayout(gg)
        gl.setSpacing(4)
        game_keys = ("项目名称", "项目目录", "UProject", "打包输出",
                     "PSO 工作目录", "平台", "SHK 相对路径", "REC 相对路径", "SPC 相对路径")
        for key in game_keys:
            lbl = QLabel("—")
            lbl.setWordWrap(True)
            lbl.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
            lbl.setStyleSheet("color: #AAAAAA; padding: 2px 6px; font-size: 12px;")
            gl.addRow(f"{key}:", lbl)
            self._mid_game_labels[key] = lbl
        cl.addWidget(gg)

        # Part 3: PSO 项目配置检查
        pg = QGroupBox("PSO 项目配置检查")
        pl = QVBoxLayout(pg)
        pl.setSpacing(2)
        self._mid_pso_checks.clear()
        for ini_filename, sections in PSO_REQUIRED_CONFIGS.items():
            fl = QLabel(f"  {ini_filename}")
            fl.setStyleSheet("color: #888888; font-size: 11px; font-weight: bold; padding: 2px 0;")
            pl.addWidget(fl)
            for section_name, configs in sections.items():
                for key, expected_value, description in configs:
                    cb = QCheckBox(f"{key} = {expected_value}  ({description})")
                    cb.setEnabled(False)
                    cb.setStyleSheet(
                        "QCheckBox { color: #AAAAAA; font-size: 12px; spacing: 6px; padding: 1px 0; }"
                        "QCheckBox::indicator { width: 14px; height: 14px; }"
                    )
                    pl.addWidget(cb)
                    self._mid_pso_checks.append(cb)
        cl.addWidget(pg)

        # Part 4: 首次打包内容
        fbg = QGroupBox("首次打包内容")
        fbl = QVBoxLayout(fbg)
        fbl.setSpacing(2)
        self._mid_first_build_rows = self._create_param_table(_FIRST_FINAL_PARAMS)
        for widget in self._mid_first_build_rows.values():
            fbl.addWidget(widget[0])  # row_widget
        cl.addWidget(fbg)

        # Part 5: 转换缓存
        cg = QGroupBox("转换缓存")
        cgl = QVBoxLayout(cg)
        cgl.setSpacing(2)
        self._mid_cache_convert_rows = self._create_param_table(_CACHE_CONVERT_PARAMS)
        for widget in self._mid_cache_convert_rows.values():
            cgl.addWidget(widget[0])  # row_widget
        cl.addWidget(cg)

        # Part 6: 最终打包
        fg = QGroupBox("最终打包")
        fgl = QVBoxLayout(fg)
        fgl.setSpacing(2)
        self._mid_final_build_rows = self._create_param_table(_FIRST_FINAL_PARAMS)
        for widget in self._mid_final_build_rows.values():
            fgl.addWidget(widget[0])  # row_widget
        cl.addWidget(fg)

        cl.addStretch()
        self._mid_scroll.setWidget(content)

        wrapper = QWidget()
        wl = QVBoxLayout(wrapper)
        wl.setContentsMargins(0, 0, 0, 0)
        wl.addWidget(self._mid_scroll)
        return wrapper

    def _create_param_table(self, param_defs: list) -> dict:
        """创建参数表格行，返回 {value_key: (row_widget, value_widget, browse_btn_or_None)}"""
        rows = {}
        for pd in param_defs:
            row_widget = QWidget()
            row = QHBoxLayout(row_widget)
            row.setContentsMargins(4, 1, 4, 1)
            row.setSpacing(4)

            # 功能描述列
            desc_lbl = QLabel(pd.desc)
            desc_lbl.setFixedWidth(72)
            desc_lbl.setStyleSheet("color: #AAAAAA; font-size: 12px;")
            desc_lbl.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
            row.addWidget(desc_lbl)

            # 参数名列
            flag_lbl = QLabel(pd.flag)
            flag_lbl.setFixedWidth(130)
            flag_lbl.setStyleSheet("color: #4FC3F7; font-size: 11px; font-family: Consolas;")
            row.addWidget(flag_lbl)

            value_widget = None
            browse_btn = None

            if pd.widget_type == "check":
                cb = QCheckBox()
                cb.setStyleSheet("QCheckBox::indicator { width: 14px; height: 14px; }")
                row.addWidget(cb)
                row.addStretch()
                rows[pd.value_key] = (row_widget, cb, None)
                continue

            # 下拉单选
            if pd.widget_type == "combo":
                cmb = QComboBox()
                cmb.setStyleSheet(
                    "QComboBox { background-color: #252525; color: #CCCCCC; border: 1px solid #3D3D3D;"
                    " border-radius: 2px; padding: 1px 6px; font-size: 11px; min-height: 22px; }"
                    "QComboBox:hover { border: 1px solid #4FC3F7; }"
                    "QComboBox QAbstractItemView { background-color: #252525; color: #CCCCCC;"
                    " border: 1px solid #3D3D3D; selection-background-color: #333333; }"
                )
                if pd.choices:
                    for display, value in pd.choices:
                        cmb.addItem(display, value)
                row.addWidget(cmb, stretch=1)
                rows[pd.value_key] = (row_widget, cmb, None)
                continue

            # 值输入框
            if pd.widget_type == "readonly":
                le = QLineEdit()
                le.setReadOnly(True)
                le.setStyleSheet(
                    "QLineEdit { background-color: #2A2A2A; color: #888888; border: 1px solid #3D3D3D;"
                    " border-radius: 2px; padding: 2px 6px; font-size: 11px; font-family: Consolas; }"
                )
            else:
                le = QLineEdit()
                le.setStyleSheet(
                    "QLineEdit { background-color: #252525; color: #CCCCCC; border: 1px solid #3D3D3D;"
                    " border-radius: 2px; padding: 2px 6px; font-size: 11px; font-family: Consolas; }"
                    "QLineEdit:focus { border: 1px solid #4FC3F7; }"
                )
            le.setMinimumHeight(22)
            row.addWidget(le, stretch=1)
            value_widget = le

            # 浏览按钮（仅路径类型）
            if pd.widget_type in ("path_file", "path_dir"):
                browse_btn = QPushButton("...")
                browse_btn.setFixedSize(24, 22)
                browse_btn.setStyleSheet(
                    "QPushButton { background-color: #333333; color: #AAAAAA; border: 1px solid #3D3D3D;"
                    " border-radius: 2px; font-size: 10px; padding: 0; }"
                    "QPushButton:hover { background-color: #444444; color: #CCCCCC; }"
                )
                if pd.widget_type == "path_file":
                    filter_str = (
                        "UProject Files (*.uproject)" if pd.value_key == "project"
                        else "SPC Files (*.spc);;All Files (*)"
                    )
                    browse_btn.clicked.connect(lambda checked, e=le, f=filter_str: self._browse_file(e, f))
                else:
                    browse_btn.clicked.connect(lambda checked, e=le: self._browse_dir(e))
                row.addWidget(browse_btn)

            rows[pd.value_key] = (row_widget, value_widget, browse_btn)

        return rows

    def _refresh_middle_panel(self):
        """刷新中间面板全部 6 组内容"""
        self._populate_engine_config()
        self._populate_game_config()
        self._populate_pso_config_check()
        self._populate_first_build_params()
        self._populate_cache_convert_params()
        self._populate_final_build_params()

    def _populate_engine_config(self):
        if not self._mid_engine_labels:
            return
        ver = self._current_ue5_version
        ue = self._config.ue5_versions.get(ver) if ver else None

        def _s(key, text, color="#AAAAAA"):
            lbl = self._mid_engine_labels.get(key)
            if lbl:
                lbl.setText(text or "—")
                lbl.setStyleSheet(f"color: {color}; padding: 2px 6px; font-size: 12px;")

        if ue:
            _s("Editor-Cmd", ue.editor_cmd_path,
               "#4ECB71" if ue.editor_cmd_path and Path(ue.editor_cmd_path).exists() else "#E0556A")
            _s("引擎目录", ue.install_dir or "—（自动推导）",
               "#4ECB71" if ue.install_dir else "#888888")
            _s("RunUAT.bat", ue.uat_bat_path or "—（自动推导）",
               "#4ECB71" if ue.uat_bat_path else "#888888")
            # 从项目的 DefaultEngine.ini 实际读取 Shader 格式
            proj = self._config.get_project(self._current_project_index)
            sf = _read_shader_formats_from_ini(proj.project_dir) if proj else "PCD3D_SM6"
            _s("Shader 格式", sf, "#4ECB71")
        else:
            for k in self._mid_engine_labels:
                _s(k, "请先选择 UE 版本", "#666666")

    def _populate_game_config(self):
        if not self._mid_game_labels:
            return
        proj = self._config.get_project(self._current_project_index)

        def _s(key, text, color="#AAAAAA"):
            lbl = self._mid_game_labels.get(key)
            if lbl:
                lbl.setText(text or "—")
                lbl.setStyleSheet(f"color: {color}; padding: 2px 6px; font-size: 12px;")

        if proj:
            _s("项目名称", proj.name, "#FFFFFF")
            _s("项目目录", proj.project_dir,
               "#4ECB71" if proj.project_dir else "#E0556A")
            _s("UProject", proj.uproject_file,
               "#4ECB71" if proj.uproject_file and Path(proj.uproject_file).exists() else "#E0556A")
            _s("打包输出", proj.output_dir,
               "#4ECB71" if proj.output_dir else "#E0556A")
            _s("PSO 工作目录", proj.pso_cache_work_dir,
               "#4ECB71" if proj.pso_cache_work_dir else "#E0556A")
            _s("平台", proj.target_platform, "#888888")
            _s("SHK 相对路径", proj.shk_source_relative, "#888888")
            _s("REC 相对路径", proj.rec_source_relative, "#888888")
            _s("SPC 相对路径", proj.spc_target_relative, "#888888")
        else:
            for k in self._mid_game_labels:
                _s(k, "请先选择项目", "#666666")

    def _populate_pso_config_check(self):
        proj = self._config.get_project(self._current_project_index)
        if not proj or not self._mid_pso_checks:
            return

        proj_dir = Path(proj.project_dir) if proj.project_dir else None
        if not proj_dir or not proj_dir.exists():
            for cb in self._mid_pso_checks:
                cb.setChecked(False)
                cb.setStyleSheet(
                    "QCheckBox { color: #666666; font-size: 12px; spacing: 6px; }"
                    "QCheckBox::indicator { width: 14px; height: 14px; }"
                )
            return

        config_dir = proj_dir / "Config"
        idx = 0
        for ini_filename, sections in PSO_REQUIRED_CONFIGS.items():
            ini_path = config_dir / ini_filename
            ini_exists = ini_path.exists()
            content = ini_path.read_text(encoding="utf-8", errors="replace") if ini_exists else ""

            for section_name, configs in sections.items():
                section_content = ""
                if ini_exists:
                    m = re.search(
                        rf'\[{re.escape(section_name)}\](.*?)(?=\[|$)',
                        content, re.DOTALL | re.IGNORECASE
                    )
                    if m:
                        section_content = m.group(1)

                for key, expected_value, description in configs:
                    if idx >= len(self._mid_pso_checks):
                        break
                    cb = self._mid_pso_checks[idx]

                    if not ini_exists or not section_content:
                        cb.setChecked(False)
                        cb.setStyleSheet(
                            "QCheckBox { color: #E0556A; font-size: 12px; spacing: 6px; }"
                            "QCheckBox::indicator { width: 14px; height: 14px; }"
                        )
                    else:
                        key_m = re.search(
                            rf'^[+\-.]?{re.escape(key)}\s*=\s*(.*)$',
                            section_content, re.MULTILINE | re.IGNORECASE
                        )
                        if key_m:
                            actual = key_m.group(1).strip()
                            if expected_value and actual.lower() != expected_value.lower():
                                cb.setChecked(False)
                                cb.setStyleSheet(
                                    "QCheckBox { color: #E0A800; font-size: 12px; spacing: 6px; }"
                                    "QCheckBox::indicator { width: 14px; height: 14px; }"
                                )
                            else:
                                cb.setChecked(True)
                                cb.setStyleSheet(
                                    "QCheckBox { color: #4ECB71; font-size: 12px; spacing: 6px; }"
                                    "QCheckBox::indicator { width: 14px; height: 14px; }"
                                )
                        else:
                            cb.setChecked(False)
                            cb.setStyleSheet(
                                "QCheckBox { color: #E0556A; font-size: 12px; spacing: 6px; }"
                                "QCheckBox::indicator { width: 14px; height: 14px; }"
                            )
                    idx += 1

    def _populate_first_build_params(self):
        """填充首次打包参数表格"""
        self._populate_uat_params(self._mid_first_build_rows)

    def _populate_final_build_params(self):
        """填充最终打包参数表格"""
        self._populate_uat_params(self._mid_final_build_rows)

    def _populate_uat_params(self, rows: dict):
        """填充 UAT BuildCookRun 参数行"""
        proj = self._config.get_project(self._current_project_index)
        ver = self._current_ue5_version
        ue = self._config.ue5_versions.get(ver) if ver else None

        if not proj or not ue:
            self._clear_param_rows(rows)
            return

        # 从 DefaultEngine.ini 实际读取 Shader 格式（不可修改）
        sf = _read_shader_formats_from_ini(proj.project_dir)

        defaults = {
            "project":       proj.uproject_file or "",
            "platform":      proj.target_platform or "Win64",
            "config":        "Development",
            "output_dir":    proj.output_dir or "",
            "shader_formats": sf,
            "unreal_exe":    ue.editor_cmd_path or "",
            "compile":       True, "build": True, "cook": True,
            "stage":         True, "pak": True, "archive": True,
            "crash_reporter": True, "utf8_output": True,
        }
        self._fill_param_rows(rows, defaults, proj, ue)

    def _populate_cache_convert_params(self):
        """填充转换缓存参数表格"""
        proj = self._config.get_project(self._current_project_index)
        ver = self._current_ue5_version
        ue = self._config.ue5_versions.get(ver) if ver else None
        rows = self._mid_cache_convert_rows

        if not proj or not ue:
            self._clear_param_rows(rows)
            return

        defaults = {
            "project":    proj.uproject_file or "",
            "run_cmd":    "ShaderPipelineCacheTools expand",
            "rec_source": proj.resolve_rec_source_dir(),
            "shk_source": proj.resolve_shk_source_dir(),
            "spc_target": proj.resolve_spc_target_dir(),
            "null_rhi":   True,
            "unattended": True,
        }
        self._fill_param_rows(rows, defaults, proj, ue)

    def _fill_param_rows(self, rows: dict, values: dict, proj, ue):
        """将值填入参数行控件"""
        for key, (row_widget, value_widget, browse_btn) in rows.items():
            val = values.get(key)
            if isinstance(value_widget, QCheckBox):
                value_widget.setChecked(bool(val))
            elif isinstance(value_widget, QComboBox):
                value_widget.blockSignals(True)
                if val is not None:
                    idx = value_widget.findData(val)
                    if idx >= 0:
                        value_widget.setCurrentIndex(idx)
                value_widget.blockSignals(False)
            elif isinstance(value_widget, QLineEdit):
                value_widget.blockSignals(True)
                if val is None:
                    value_widget.setText("")
                    value_widget.setPlaceholderText("（请先完整配置）")
                elif isinstance(val, bool):
                    value_widget.setText(str(val))
                else:
                    value_widget.setText(str(val))
                value_widget.blockSignals(False)
            row_widget.setVisible(True)

    def _clear_param_rows(self, rows: dict):
        """清空参数行内容"""
        for key, (row_widget, value_widget, browse_btn) in rows.items():
            if isinstance(value_widget, QCheckBox):
                value_widget.setChecked(False)
            elif isinstance(value_widget, QComboBox):
                value_widget.blockSignals(True)
                value_widget.setCurrentIndex(0)
                value_widget.blockSignals(False)
            elif isinstance(value_widget, QLineEdit):
                value_widget.blockSignals(True)
                value_widget.setText("")
                value_widget.setPlaceholderText("（请先完整配置项目和 UE 路径）")
                value_widget.blockSignals(False)

    # ============================================================
    # 现有方法
    # ============================================================

    def refresh(self):
        """从 ConfigManager 刷新全部 UI 数据"""
        self._refresh_ue_list()
        self._refresh_ue_versions_combo()
        self._refresh_proj_list()
        self._refresh_middle_panel()

    def _refresh_ue_list(self):
        self._ue_list.blockSignals(True)
        self._ue_list.clear()
        for ver in self._config.ue5_versions.keys():
            self._ue_list.addItem(ver)
        self._ue_list.blockSignals(False)

        if self._current_ue5_version and self._current_ue5_version in self._config.ue5_versions:
            self._ue_list.setCurrentRow(
                list(self._config.ue5_versions.keys()).index(self._current_ue5_version))
            self._render_ue_detail(self._current_ue5_version)
        elif self._ue_list.count() > 0:
            self._ue_list.setCurrentRow(0)

    def _refresh_ue_versions_combo(self):
        self._proj_ue_version.blockSignals(True)
        self._proj_ue_version.clear()
        for ver in self._config.ue5_versions.keys():
            self._proj_ue_version.addItem(ver)
        self._proj_ue_version.blockSignals(False)

    def _refresh_proj_list(self):
        self._proj_list.blockSignals(True)
        self._proj_list.clear()
        for proj in self._config.projects:
            item = QListWidgetItem(f"{proj.name}")
            item.setData(Qt.ItemDataRole.UserRole, proj.name)
            self._proj_list.addItem(item)
        self._proj_list.blockSignals(False)

        if 0 <= self._current_project_index < len(self._config.projects):
            self._proj_list.setCurrentRow(self._current_project_index)
            self._render_proj_detail(self._current_project_index)
        elif self._proj_list.count() > 0:
            self._proj_list.setCurrentRow(0)

    # ---- UE 表单 ----

    def _on_ue_selected(self, version: str):
        if not version:
            return
        self._current_ue5_version = version
        self._render_ue_detail(version)
        self._populate_engine_config()
        self._populate_first_build_params()
        self._populate_cache_convert_params()
        self._populate_final_build_params()

    def _render_ue_detail(self, version: str):
        ue = self._config.ue5_versions.get(version)
        if not ue:
            return

        self._ue_editor_cmd_path.blockSignals(True)
        self._ue_editor_cmd_path.setText(ue.editor_cmd_path)
        self._ue_editor_cmd_path.blockSignals(False)

        self._show_derived_paths(ue)
        self._update_ue_status()

    def _show_derived_paths(self, ue: UE5VersionConfig):
        if ue.install_dir:
            self._ue_install_label.setText(ue.install_dir)
            self._ue_install_label.setStyleSheet("color: #4ECB71; padding: 6px 8px;")
        else:
            self._ue_install_label.setText("— 请先选择 UnrealEditor-Cmd.exe")
            self._ue_install_label.setStyleSheet("color: #888888; padding: 6px 8px;")

        if ue.uat_bat_path:
            exists = Path(ue.uat_bat_path).exists()
            color = "#4ECB71" if exists else "#E0556A"
            self._ue_uat_label.setText(ue.uat_bat_path)
            self._ue_uat_label.setStyleSheet(f"color: {color}; padding: 6px 8px;")
        else:
            self._ue_uat_label.setText("—")
            self._ue_uat_label.setStyleSheet("color: #888888; padding: 6px 8px;")

    def _update_ue_status(self):
        ver = self._current_ue5_version
        ue = self._config.ue5_versions.get(ver)
        if not ue:
            self._ue_status.setText("")
            return

        if not ue.editor_cmd_path:
            self._ue_status.setText("⚠ 请选择 UnrealEditor-Cmd.exe 路径")
            self._ue_status.setStyleSheet("color: #E0A800;")
            return

        errors = ue.validate()
        if not errors:
            self._ue_status.setText("✓ 路径验证通过")
            self._ue_status.setStyleSheet("color: #4ECB71;")
        else:
            self._ue_status.setText("✗ " + errors[0])
            self._ue_status.setStyleSheet("color: #E0556A;")

    def _on_editor_cmd_changed(self, text: str):
        if not text or not self._current_ue5_version:
            return
        ue = self._config.ue5_versions.get(self._current_ue5_version)
        if not ue:
            return
        ue.editor_cmd_path = text.strip()
        ue.resolve_from_editor_cmd()
        self._show_derived_paths(ue)
        self._update_ue_status()

    def _on_add_ue(self):
        dlg = QDialog(self)
        dlg.setWindowTitle("新增 UE 版本")
        layout = QFormLayout(dlg)
        edit = QLineEdit()
        edit.setPlaceholderText("例如: 5.6")
        layout.addRow("版本号:", edit)
        btns = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        btns.accepted.connect(dlg.accept)
        btns.rejected.connect(dlg.reject)
        layout.addRow(btns)

        if dlg.exec() == QDialog.DialogCode.Accepted:
            ver = edit.text().strip()
            if ver and ver not in self._config.ue5_versions:
                self._config.add_ue5_version(ver, UE5VersionConfig())
                self._current_ue5_version = ver
                self._refresh_ue_list()
                self._refresh_ue_versions_combo()
                self._ue_list.setCurrentRow(self._ue_list.count() - 1)

    def _on_del_ue(self):
        ver = self._current_ue5_version
        if not ver:
            return
        ok = self._config.remove_ue5_version(ver)
        if not ok:
            QMessageBox.warning(self, "无法删除", f"UE 版本 '{ver}' 被项目引用，请先删除关联项目")
            return
        self._current_ue5_version = ""
        self._refresh_ue_list()
        self._refresh_ue_versions_combo()
        self._clear_ue_form()

    def _clear_ue_form(self):
        self._ue_editor_cmd_path.clear()
        self._ue_install_label.setText("—")
        self._ue_install_label.setStyleSheet("color: #888888; padding: 6px 8px;")
        self._ue_uat_label.setText("—")
        self._ue_uat_label.setStyleSheet("color: #888888; padding: 6px 8px;")
        self._ue_status.clear()

    # ---- 项目表单 ----

    def _on_proj_selected(self, index: int):
        if index < 0:
            return
        self._current_project_index = index
        self._render_proj_detail(index)
        self._populate_game_config()
        self._populate_pso_config_check()
        self._populate_first_build_params()
        self._populate_cache_convert_params()
        self._populate_final_build_params()

    def _render_proj_detail(self, index: int):
        proj = self._config.get_project(index)
        if not proj:
            return

        self._proj_name.blockSignals(True)
        self._proj_dir.blockSignals(True)
        self._proj_uproject.blockSignals(True)
        self._proj_output.blockSignals(True)
        self._proj_pso_work.blockSignals(True)
        self._proj_shk_rel.blockSignals(True)
        self._proj_rec_rel.blockSignals(True)
        self._proj_spc_rel.blockSignals(True)

        self._proj_name.setText(proj.name)
        self._proj_dir.setText(proj.project_dir)
        self._proj_uproject.setText(proj.uproject_file)
        self._proj_output.setText(proj.output_dir)
        self._proj_pso_work.setText(proj.pso_cache_work_dir)
        self._proj_shk_rel.setText(proj.shk_source_relative)
        self._proj_rec_rel.setText(proj.rec_source_relative)
        self._proj_spc_rel.setText(proj.spc_target_relative)

        idx = self._proj_ue_version.findText(proj.ue5_version)
        if idx >= 0:
            self._proj_ue_version.setCurrentIndex(idx)

        self._proj_name.blockSignals(False)
        self._proj_dir.blockSignals(False)
        self._proj_uproject.blockSignals(False)
        self._proj_output.blockSignals(False)
        self._proj_pso_work.blockSignals(False)
        self._proj_shk_rel.blockSignals(False)
        self._proj_rec_rel.blockSignals(False)
        self._proj_spc_rel.blockSignals(False)

        self._show_ue_match_status()
        self._update_proj_status()

    def _update_proj_status(self):
        index = self._current_project_index
        proj = self._config.get_project(index)
        if not proj:
            self._proj_status.setText("")
            return

        errors = proj.validate()
        if not errors:
            self._proj_status.setText("✓ 配置验证通过")
            self._proj_status.setStyleSheet("color: #4ECB71;")
        else:
            self._proj_status.setText("✗ " + errors[0])
            self._proj_status.setStyleSheet("color: #E0556A;")

    def _on_add_proj(self):
        proj = ProjectConfig()
        proj.name = f"NewProject{len(self._config.projects) + 1}"
        if self._proj_ue_version.count() > 0:
            proj.ue5_version = self._proj_ue_version.currentText()
        index = self._config.add_project(proj)
        self._current_project_index = index
        self._refresh_proj_list()
        self._proj_list.setCurrentRow(index)

    def _on_del_proj(self):
        index = self._current_project_index
        if index < 0:
            return
        proj = self._config.get_project(index)
        name = proj.name if proj else ""

        reply = QMessageBox.question(
            self, "确认删除",
            f"确定要删除项目 '{name}' 吗？",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        )
        if reply != QMessageBox.StandardButton.Yes:
            return

        self._config.remove_project(index)
        self._current_project_index = -1
        self._refresh_proj_list()
        self._clear_proj_form()

    def _clear_proj_form(self):
        self._proj_name.clear()
        self._proj_dir.clear()
        self._proj_uproject.clear()
        self._proj_output.clear()
        self._proj_pso_work.clear()
        self._proj_ue_status.clear()
        self._proj_status.clear()

    # ---- 从 .uproject 读取 EngineAssociation ----

    def _on_uproject_changed(self, text: str):
        path = text.strip()
        if not path:
            self._proj_ue_status.clear()
            return

        uproject_path = Path(path)
        if not uproject_path.exists() or uproject_path.suffix.lower() != ".uproject":
            self._proj_ue_status.clear()
            return

        proj_dir = str(uproject_path.parent)
        if self._proj_dir.text().strip() == "":
            self._proj_dir.blockSignals(True)
            self._proj_dir.setText(proj_dir)
            self._proj_dir.blockSignals(False)

        engine_assoc = _read_engine_association(uproject_path)
        if engine_assoc is None:
            self._proj_ue_status.setText("⚠ 无法读取 .uproject 文件")
            self._proj_ue_status.setStyleSheet("color: #E0556A; padding: 2px 8px;")
            return
        if not engine_assoc:
            self._proj_ue_status.setText("⚠ .uproject 中未找到 EngineAssociation")
            self._proj_ue_status.setStyleSheet("color: #E0A800; padding: 2px 8px;")
            return

        if engine_assoc.startswith("{") and engine_assoc.endswith("}"):
            self._proj_ue_status.setText(f"⚙ 自定义引擎构建: {engine_assoc}")
            self._proj_ue_status.setStyleSheet("color: #888888; padding: 2px 8px;")
            return

        idx = self._proj_ue_version.findText(engine_assoc)
        if idx >= 0:
            self._proj_ue_version.setCurrentIndex(idx)
            self._proj_ue_status.setText(f"✓ 已自动匹配 UE {engine_assoc}")
            self._proj_ue_status.setStyleSheet("color: #4ECB71; padding: 2px 8px;")
        else:
            self._proj_ue_status.setText(f"⚠ 引擎版本 '{engine_assoc}' 未在左侧配置，请先添加")
            self._proj_ue_status.setStyleSheet("color: #E0A800; padding: 2px 8px;")

    def _show_ue_match_status(self):
        path = self._proj_uproject.text().strip()
        if not path:
            self._proj_ue_status.clear()
            return

        uproject_path = Path(path)
        engine_assoc = _read_engine_association(uproject_path)
        if engine_assoc is None:
            self._proj_ue_status.setText("⚠ 无法读取 .uproject 文件")
            self._proj_ue_status.setStyleSheet("color: #E0556A; padding: 2px 8px;")
            return
        if not engine_assoc:
            self._proj_ue_status.setText("⚠ .uproject 中未找到 EngineAssociation")
            self._proj_ue_status.setStyleSheet("color: #E0A800; padding: 2px 8px;")
            return

        if engine_assoc.startswith("{") and engine_assoc.endswith("}"):
            self._proj_ue_status.setText(f"⚙ 自定义引擎构建: {engine_assoc}")
            self._proj_ue_status.setStyleSheet("color: #888888; padding: 2px 8px;")
            return

        current_ver = self._proj_ue_version.currentText()
        if current_ver == engine_assoc:
            self._proj_ue_status.setText(f"✓ .uproject 引擎版本与配置一致 (UE {engine_assoc})")
            self._proj_ue_status.setStyleSheet("color: #4ECB71; padding: 2px 8px;")
        else:
            self._proj_ue_status.setText(f"⚠ .uproject 引擎版本为 '{engine_assoc}'，当前选择为 '{current_ver}'")
            self._proj_ue_status.setStyleSheet("color: #E0A800; padding: 2px 8px;")

    def _on_save(self):
        """将当前表单数据写回 ConfigManager 并持久化"""
        ver = self._current_ue5_version
        if ver and ver in self._config.ue5_versions:
            ue = self._config.ue5_versions[ver]
            ue.editor_cmd_path = self._ue_editor_cmd_path.text().strip()
            ue.resolve_from_editor_cmd()

        index = self._current_project_index
        proj = self._config.get_project(index)
        if proj:
            proj.name = self._proj_name.text().strip()
            proj.ue5_version = self._proj_ue_version.currentText()
            proj.project_dir = self._proj_dir.text().strip()
            proj.uproject_file = self._proj_uproject.text().strip()
            proj.output_dir = self._proj_output.text().strip()
            proj.pso_cache_work_dir = self._proj_pso_work.text().strip()
            proj.shk_source_relative = self._proj_shk_rel.text().strip()
            proj.rec_source_relative = self._proj_rec_rel.text().strip()
            proj.spc_target_relative = self._proj_spc_rel.text().strip()

        if self._config.save():
            self._refresh_ue_list()
            self._refresh_proj_list()
            self._update_ue_status()
            self._update_proj_status()
            self.config_changed.emit()
            QMessageBox.information(self, "保存成功", "配置已保存到:\n" + str(self._config._config_path))
        else:
            QMessageBox.critical(self, "保存失败", "无法写入配置文件")

    # ---- 工具 ----

    def _browse_dir(self, edit: QLineEdit):
        path = QFileDialog.getExistingDirectory(self, "选择目录", edit.text())
        if path:
            edit.setText(path)

    def _browse_file(self, edit: QLineEdit, filter_str: str):
        path, _ = QFileDialog.getOpenFileName(self, "选择文件", edit.text(), filter_str)
        if path:
            edit.setText(path)