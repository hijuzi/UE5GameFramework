"""
BuildParamsTab - 打包参数标签页
包含：首次打包内容、转换缓存、最终打包 三组参数预览
每部分底部有独立执行按钮
"""
import os
import subprocess
from pathlib import Path

from datetime import datetime

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QLineEdit, QPushButton, QLabel, QSplitter,
    QComboBox, QFileDialog, QScrollArea, QCheckBox,
    QPlainTextEdit, QSizePolicy,
)
from PySide6.QtCore import Qt, Signal, QThread
from PySide6.QtGui import QWheelEvent

from config_manager import ConfigManager, ProjectConfig, UE5VersionConfig, read_shader_formats_list_from_ini
from step_runner import StepRunner


class _WorkerThread(QThread):
    """简单的 Worker 线程：只执行 target，runner 始终留在主线程，避免 moveToThread 线程亲和性问题。"""
    def __init__(self, target, parent=None):
        super().__init__(parent)
        self._target = target

    def run(self):
        self._target()
from ui.config_tab import (
    _FIRST_FINAL_PARAMS, _CACHE_CONVERT_PARAMS, _PSO_COLLECT_PARAMS,
    _create_param_table, _fill_param_rows, _clear_param_rows,
    _browse_dir, _browse_file,
)

# 步骤索引映射
_STEP_FIRST_BUILD = 2    # Step 2: 首次打包
_STEP_COLLECT_PSO = 3    # Step 3: 收集 PSO
_STEP_CACHE_CONVERT = 6  # Step 6: 转换缓存
_STEP_FINAL_BUILD = 8    # Step 8: 最终打包


class BuildParamsTab(QWidget):
    """打包参数标签页：独立展示首次打包 / 转换缓存 / 最终打包 的参数"""

    def __init__(self, config: ConfigManager, runner: StepRunner, parent=None):
        super().__init__(parent)
        self._config = config
        self._runner = runner
        self._current_project_index: int = -1
        self._current_ue5_version: str = ""

        # 参数表格行
        self._first_build_rows: dict[str, tuple] = {}
        self._pso_collect_rows: dict[str, tuple] = {}
        self._cache_convert_rows: dict[str, tuple] = {}
        self._final_build_rows: dict[str, tuple] = {}

        # 执行状态
        self._running = False
        self._thread: QThread | None = None
        self._last_step_type: str = ""   # "build" | "cache" | ""  用于控制结果按钮可见性

        self._setup_ui()

    _STEP_NAMES = {2: "首次打包", 3: "PSO 收集", 6: "转换缓存", 8: "最终打包"}

    # ---- 按钮配色与图标 ----
    _BTN_STYLE = {
        "first":  {"icon": "\U0001F3D7\uFE0F ", "color": "#0078D4", "hover": "#1A8CE8", "press": "#005A9E",
                   "text": "执行首次打包", "running_text": "\u23F3 执行中..."},
        "pso":    {"icon": "\U0001F3AE ",       "color": "#E67E22", "hover": "#F39C12", "press": "#D35400",
                   "text": "启动收集PSO游戏", "running_text": "\u23F3 启动中..."},
        "cache":  {"icon": "\U0001F504 ",       "color": "#00A88F", "hover": "#00C7AE", "press": "#008A73",
                   "text": "执行缓存转换", "running_text": "\u23F3 执行中..."},
        "final":  {"icon": "\U0001F4E6 ",       "color": "#8B5CF6", "hover": "#A78BFA", "press": "#6D3FD6",
                   "text": "执行最终打包", "running_text": "\u23F3 执行中..."},
    }

    def _setup_ui(self):
        root = QHBoxLayout(self)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(0)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.setHandleWidth(3)
        splitter.setStyleSheet("QSplitter::handle { background-color: #3D3D3D; border-radius: 1px; }")

        # ===================================================================
        # 左侧：参数区 (stretch=5)
        # ===================================================================
        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(0, 0, 8, 0)
        left_layout.setSpacing(0)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("QScrollArea { border: none; background-color: #1E1E1E; }")

        content = QWidget()
        cl = QVBoxLayout(content)
        cl.setContentsMargins(0, 0, 6, 0)
        cl.setSpacing(14)

        # --- 参数面板通用样式 ---
        gb_style = """
            QGroupBox {
                border: 1px solid #3A3A3A;
                border-radius: 8px;
                margin-top: 14px;
                padding: 20px 10px 10px 10px;
                font-weight: bold;
                font-size: 13px;
                color: #E0E0E0;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 12px;
                padding: 0 8px;
                color: #CCCCCC;
            }
        """

        # Part 1: 首次打包内容
        fbg = QGroupBox("  首次打包内容")
        fbg.setStyleSheet(gb_style)
        fbl = QVBoxLayout(fbg)
        fbl.setSpacing(2)
        self._first_build_rows = _create_param_table(_FIRST_FINAL_PARAMS, self, font_scale=1.2)
        for widget in self._first_build_rows.values():
            fbl.addWidget(widget[0])
        cl.addWidget(fbg)

        # Part 2: PSO 收集参数
        psog = QGroupBox("  PSO 收集")
        psog.setStyleSheet(gb_style)
        psol = QVBoxLayout(psog)
        psol.setSpacing(2)
        self._pso_collect_rows = _create_param_table(_PSO_COLLECT_PARAMS, self, font_scale=1.2)
        for widget in self._pso_collect_rows.values():
            psol.addWidget(widget[0])
        cl.addWidget(psog)

        # Part 3: 转换缓存
        cg = QGroupBox("  转换缓存")
        cg.setStyleSheet(gb_style)
        cgl = QVBoxLayout(cg)
        cgl.setSpacing(2)
        self._cache_convert_rows = _create_param_table(_CACHE_CONVERT_PARAMS, self, font_scale=1.2)
        for widget in self._cache_convert_rows.values():
            cgl.addWidget(widget[0])
        cl.addWidget(cg)

        # Part 4: 最终打包
        fg = QGroupBox("  最终打包")
        fg.setStyleSheet(gb_style)
        fgl = QVBoxLayout(fg)
        fgl.setSpacing(2)
        self._final_build_rows = _create_param_table(_FIRST_FINAL_PARAMS, self, font_scale=1.2)
        for widget in self._final_build_rows.values():
            fgl.addWidget(widget[0])
        cl.addWidget(fg)

        cl.addStretch()
        scroll.setWidget(content)
        left_layout.addWidget(scroll)
        splitter.addWidget(left)

        # ===================================================================
        # 右侧：操作区 (stretch=3, 更宽)
        # ===================================================================
        right = QWidget()
        right_layout = QVBoxLayout(right)
        right_layout.setContentsMargins(8, 0, 0, 0)
        right_layout.setSpacing(14)

        # ---- 任务操作卡片 ----
        btn_card = QGroupBox("  任务操作")
        btn_card.setStyleSheet(gb_style)
        btn_card_layout = QVBoxLayout(btn_card)
        btn_card_layout.setContentsMargins(10, 18, 10, 12)
        btn_card_layout.setSpacing(8)

        # 四个按钮 2x2 布局，独立配色
        btn_row1 = QHBoxLayout()
        btn_row1.setSpacing(8)

        self._btn_first_build = self._make_action_btn("first")
        self._btn_first_build.clicked.connect(self._on_run_first_build)
        btn_row1.addWidget(self._btn_first_build)

        self._btn_pso_collect = self._make_action_btn("pso")
        self._btn_pso_collect.clicked.connect(self._on_launch_pso_collection)
        btn_row1.addWidget(self._btn_pso_collect)

        btn_card_layout.addLayout(btn_row1)

        btn_row2 = QHBoxLayout()
        btn_row2.setSpacing(8)

        self._btn_cache_convert = self._make_action_btn("cache")
        self._btn_cache_convert.clicked.connect(self._on_run_cache_convert)
        btn_row2.addWidget(self._btn_cache_convert)

        self._btn_final_build = self._make_action_btn("final")
        self._btn_final_build.clicked.connect(self._on_run_final_build)
        btn_row2.addWidget(self._btn_final_build)

        btn_card_layout.addLayout(btn_row2)
        right_layout.addWidget(btn_card)

        # ---- 执行详情卡片 ----
        detail_card = QGroupBox("  执行详情")
        detail_card.setStyleSheet(gb_style)
        detail_layout = QVBoxLayout(detail_card)
        detail_layout.setContentsMargins(8, 18, 8, 8)

        self._log_output = QPlainTextEdit()
        self._log_output.setReadOnly(True)
        self._log_output.setMinimumHeight(200)
        self._log_output.setStyleSheet("""
            QPlainTextEdit {
                background-color: #111111;
                color: #CCCCCC;
                border: 1px solid #333333;
                border-radius: 6px;
                padding: 8px;
                font-family: "Cascadia Code", Consolas, "Microsoft YaHei", monospace;
                font-size: 12.5px;
            }
        """)
        self._log_output.setPlaceholderText("点击上方按钮执行对应步骤，此处显示实时输出…")
        self._log_output.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        detail_layout.addWidget(self._log_output, 1)
        right_layout.addWidget(detail_card, 1)  # stretch=1: 上下拓展拉伸

        # ---- 执行结果卡片（完成后显示） ----
        self._result_card = QGroupBox("  执行结果")
        self._result_card.setStyleSheet(gb_style)
        result_layout = QVBoxLayout(self._result_card)
        result_layout.setContentsMargins(12, 18, 12, 14)
        result_layout.setSpacing(10)

        # 结果头部：状态图标 + 摘要
        self._result_header = QLabel()
        self._result_header.setWordWrap(True)
        self._result_header.setStyleSheet("font-size: 14px; font-weight: bold; padding: 0; background: transparent;")
        result_layout.addWidget(self._result_header)

        # 分隔线
        sep = QLabel()
        sep.setFixedHeight(1)
        sep.setStyleSheet("background-color: #3A3A3A;")
        result_layout.addWidget(sep)

        # 详情列表
        self._result_body = QLabel()
        self._result_body.setWordWrap(True)
        self._result_body.setStyleSheet(
            "font-size: 12px; padding: 0; background: transparent; line-height: 1.6;"
        )
        result_layout.addWidget(self._result_body)

        # 结果操作按钮（首次/最终打包完成后显示）
        self._result_actions = QWidget()
        actions_layout = QHBoxLayout(self._result_actions)
        actions_layout.setContentsMargins(0, 6, 0, 0)
        actions_layout.setSpacing(8)

        self._btn_launch_game = self._make_result_action_btn("🎮 运行游戏", "#0078D4", "#1A8CE8", "#005A9E")
        self._btn_launch_game.clicked.connect(self._on_launch_game)
        actions_layout.addWidget(self._btn_launch_game)

        self._btn_close_game = self._make_result_action_btn("✕ 关闭游戏", "#DC3545", "#E0556A", "#B02A37")
        self._btn_close_game.clicked.connect(self._on_close_game)
        actions_layout.addWidget(self._btn_close_game)

        self._btn_open_dir = self._make_result_action_btn("📂 打开目录", "#3A3A3A", "#4A4A4A", "#2A2A2A")
        self._btn_open_dir.clicked.connect(self._on_open_exe_dir)
        actions_layout.addWidget(self._btn_open_dir)

        # 缓存专用按钮（替换以上按钮显示）
        self._btn_open_rec_dir = self._make_result_action_btn("📂 REC源文件目录", "#3A3A3A", "#4A4A4A", "#2A2A2A")
        self._btn_open_rec_dir.clicked.connect(lambda: self._on_open_cache_dir("rec"))
        self._btn_open_rec_dir.setVisible(False)
        actions_layout.addWidget(self._btn_open_rec_dir)

        self._btn_open_shk_dir = self._make_result_action_btn("📂 SHK源文件目录", "#3A3A3A", "#4A4A4A", "#2A2A2A")
        self._btn_open_shk_dir.clicked.connect(lambda: self._on_open_cache_dir("shk"))
        self._btn_open_shk_dir.setVisible(False)
        actions_layout.addWidget(self._btn_open_shk_dir)

        self._btn_open_spc_dir = self._make_result_action_btn("📂 目标SPC目录", "#3A3A3A", "#4A4A4A", "#2A2A2A")
        self._btn_open_spc_dir.clicked.connect(lambda: self._on_open_cache_dir("spc"))
        self._btn_open_spc_dir.setVisible(False)
        actions_layout.addWidget(self._btn_open_spc_dir)

        self._result_actions.setVisible(False)
        result_layout.addWidget(self._result_actions)

        self._result_card.setMinimumHeight(230)   # 整体高度 +50px
        self._result_card.setVisible(False)
        right_layout.addWidget(self._result_card)  # 靠底部排版

        splitter.addWidget(right)

        splitter.setStretchFactor(0, 5)
        splitter.setStretchFactor(1, 3)
        root.addWidget(splitter)

    def _make_action_btn(self, key: str) -> QPushButton:
        """创建带独立配色的操作按钮"""
        s = self._BTN_STYLE[key]
        btn = QPushButton(f"{s['icon']}{s['text']}")
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.setMinimumHeight(38)
        btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {s['color']};
                color: #FFFFFF;
                border: none;
                border-radius: 6px;
                padding: 8px 10px;
                font-size: 13px;
                font-weight: bold;
            }}
            QPushButton:hover {{
                background-color: {s['hover']};
            }}
            QPushButton:pressed {{
                background-color: {s['press']};
            }}
            QPushButton:disabled {{
                background-color: #2D2D2D;
                color: #666666;
            }}
        """)
        return btn

    def refresh(self, project_index: int, ue5_version: str):
        """根据当前选中的项目和 UE 版本刷新四组参数"""
        self._current_project_index = project_index
        self._current_ue5_version = ue5_version
        self._populate_first_build_params()
        self._populate_pso_collect_params()
        self._populate_cache_convert_params()
        self._populate_final_build_params()

    # ---- 首次 / 最终打包参数填充 ----

    def _populate_first_build_params(self):
        self._populate_uat_params(self._first_build_rows)

    def _populate_pso_collect_params(self):
        """填充 PSO 收集参数表格（exe 路径 + 复选框）"""
        proj = self._config.get_project(self._current_project_index)
        ver = self._current_ue5_version
        ue = self._config.ue5_versions.get(ver) if ver else None
        rows = self._pso_collect_rows

        if not proj or not ue:
            _clear_param_rows(rows)
            return

        self._runner.set_project(self._current_project_index)
        exe_path = self._runner._get_expected_exe_path()

        defaults = {
            "game_exe": exe_path,
            "auto_coverage": True,
            "auto_quit": True,
        }
        _fill_param_rows(rows, defaults)

    def _populate_final_build_params(self):
        self._populate_uat_params(self._final_build_rows)

    def _populate_uat_params(self, rows: dict):
        """填充 UAT BuildCookRun 参数行"""
        proj = self._config.get_project(self._current_project_index)
        ver = self._current_ue5_version
        ue = self._config.ue5_versions.get(ver) if ver else None

        if not proj or not ue:
            _clear_param_rows(rows)
            return

        # 从 DefaultEngine.ini 读取 Shader 格式列表并动态填充下拉选项
        sf_list = read_shader_formats_list_from_ini(proj.project_dir)
        if "shader_formats" in rows:
            _, shader_cmb, _ = rows["shader_formats"]
            if isinstance(shader_cmb, QComboBox):
                shader_cmb.blockSignals(True)
                current = shader_cmb.currentData()
                shader_cmb.clear()
                for fmt in sf_list:
                    shader_cmb.addItem(fmt, fmt)
                idx = shader_cmb.findData(current)
                if idx >= 0:
                    shader_cmb.setCurrentIndex(idx)
                shader_cmb.blockSignals(False)

        default_sf = sf_list[0] if sf_list else "PCD3D_SM6"

        defaults = {
            "project":       proj.uproject_file or "",
            "platform":      proj.target_platform or "Win64",
            "config":        "Development",
            "output_dir":    proj.output_dir or "",
            "shader_formats": default_sf,
            "unreal_exe":    ue.editor_cmd_path or "",
            "compile":       True, "build": True, "cook": True,
            "stage":         True, "pak": True, "archive": True,
            "crash_reporter": True, "utf8_output": True,
        }
        _fill_param_rows(rows, defaults)

    # ---- 转换缓存参数填充 ----

    def _populate_cache_convert_params(self):
        """填充转换缓存参数表格"""
        proj = self._config.get_project(self._current_project_index)
        ver = self._current_ue5_version
        ue = self._config.ue5_versions.get(ver) if ver else None
        rows = self._cache_convert_rows

        if not proj or not ue:
            _clear_param_rows(rows)
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
        _fill_param_rows(rows, defaults)

    # ---- 独立执行按钮 ----

    # step_index → rows 属性名 映射
    _ROWS_BY_STEP = {
        _STEP_FIRST_BUILD: "_first_build_rows",
        _STEP_COLLECT_PSO: "_pso_collect_rows",
        _STEP_CACHE_CONVERT: "_cache_convert_rows",
        _STEP_FINAL_BUILD: "_final_build_rows",
    }

    def _collect_ui_params(self, rows: dict) -> dict:
        """从 UI 参数行收集全部值（key=value_key）
        新增参数无需修改此处，自动纳入收集"""
        params = {}
        for key, (row_widget, value_widget, browse_btn) in rows.items():
            if isinstance(value_widget, QCheckBox):
                params[key] = value_widget.isChecked()
            elif isinstance(value_widget, QComboBox):
                if value_widget.currentIndex() >= 0:
                    params[key] = value_widget.currentData()
            elif isinstance(value_widget, QLineEdit):
                params[key] = value_widget.text()
        return params

    def _on_run_first_build(self):
        self._start_run(_STEP_FIRST_BUILD)

    def _on_run_pso_collection(self):
        self._start_run(_STEP_COLLECT_PSO)

    def _on_launch_pso_collection(self):
        """启动 PSO 收集游戏并更新执行详情 / 日志 / 工作流"""
        proj = self._config.get_project(self._current_project_index)
        if not proj:
            return
        self._runner.set_project(self._current_project_index)
        # 收集 UI 参数传给 runner
        self._runner.set_ui_params(self._collect_ui_params(self._pso_collect_rows))

        # 清空日志窗口并显示步骤头
        self._log_output.clear()
        self._result_card.setVisible(False)
        step_name = self._STEP_NAMES.get(_STEP_COLLECT_PSO, "PSO 收集")
        ts = datetime.now().strftime("%H:%M:%S")
        self._log_output.appendPlainText(f"══ {step_name} ══")
        self._log_output.appendPlainText(f"[{ts}] 正在启动…\n")

        # 临时连接 runner 日志信号以捕获启动日志
        self._runner.log_signal.connect(self._on_launch_log)

        self._runner.launch_pso_collection_game()

        # 断开临时信号
        try:
            self._runner.log_signal.disconnect(self._on_launch_log)
        except (TypeError, RuntimeError):
            pass

    def _on_launch_log(self, level: str, message: str):
        """捕获 launch_pso_collection_game 的日志输出到执行详情"""
        color = {"SUCCESS": "#4ECB71", "ERROR": "#E0556A", "WARNING": "#D4A843", "INFO": "#AAAAAA"}.get(level, "#CCCCCC")
        self._log_output.appendHtml(f'<span style="color:{color}">{message}</span>')

    def _on_run_cache_convert(self):
        self._start_run(_STEP_CACHE_CONVERT)

    def _on_run_final_build(self):
        self._start_run(_STEP_FINAL_BUILD)

    def _start_run(self, step_index: int):
        """在后台线程中执行指定步骤"""
        if self._running:
            return
        self._runner.set_project(self._current_project_index)

        # 从 UI 收集全部打包参数并传给 runner（新增参数无需修改此处）
        rows_attr = self._ROWS_BY_STEP.get(step_index)
        if rows_attr:
            rows = getattr(self, rows_attr, {})
            self._runner.set_ui_params(self._collect_ui_params(rows))

        self._runner._pending_step = step_index

        # 清空日志窗口并隐藏上次结果
        self._log_output.clear()
        self._result_card.setVisible(False)
        step_name = self._STEP_NAMES.get(step_index, f"Step {step_index}")
        ts = datetime.now().strftime("%H:%M:%S")
        self._log_output.appendPlainText(f"══ {step_name} ══")
        self._log_output.appendPlainText(f"[{ts}] 正在启动…\n")

        self._set_running(True)

        # 动态连接 runner 信号（仅本次执行期间）
        self._runner.log_signal.connect(self._on_log_line)
        self._runner.step_status_signal.connect(self._on_detail_status)
        self._runner.step_result_signal.connect(self._on_step_completed)

        # 使用 QThread.run() 子类：runner 留在主线程，worker 线程只调用其方法
        self._thread = _WorkerThread(self._runner._execute_pending_step)
        self._thread.finished.connect(self._on_thread_finished)
        self._thread.start()

    def _on_log_line(self, level: str, message: str):
        """runner 日志行 → 详情窗口（仅显示关键信息：SUCCESS / WARNING / ERROR）"""
        if level == "INFO":
            return
        color = {"SUCCESS": "#4ECB71", "ERROR": "#E0556A", "WARNING": "#D4A843"}.get(level, "#CCCCCC")
        self._log_output.appendHtml(f'<span style="color:{color}">{message}</span>')

    def _on_detail_status(self, step_index: int, status_value: int):
        """步骤状态变更"""
        from step_definitions import StepStatus
        icon = {StepStatus.RUNNING.value: "▶", StepStatus.SUCCESS.value: "✓",
                StepStatus.FAILED.value: "✗"}.get(status_value, "")
        status_name = {StepStatus.RUNNING.value: "执行中", StepStatus.SUCCESS.value: "成功",
                       StepStatus.FAILED.value: "失败"}.get(status_value, "")
        ts = datetime.now().strftime("%H:%M:%S")
        self._log_output.appendPlainText(f"[{ts}] {icon} {status_name}")
        scrollbar = self._log_output.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

    def _on_step_completed(self, step_index: int, result_text: str):
        """步骤执行完毕 —— 断开信号 + 展示结果卡片"""
        # 断开信号
        for sig, slot in [(self._runner.log_signal, self._on_log_line),
                          (self._runner.step_status_signal, self._on_detail_status),
                          (self._runner.step_result_signal, self._on_step_completed)]:
            try:
                sig.disconnect(slot)
            except (TypeError, RuntimeError):
                pass

        # 解析 result_text：第一行 = 摘要，后续 = detail HTML
        parts = result_text.split("\n", 1)
        summary_line = parts[0].strip()
        detail_html = parts[1].strip() if len(parts) > 1 else ""

        # 判断成功/失败
        is_success = summary_line.startswith("成功")
        status_color = "#4ECB71" if is_success else "#E0556A"
        status_icon = "✓" if is_success else "✗"

        # 头部：状态图标 + 摘要
        self._result_header.setText(
            f'<span style="color:{status_color}; font-size:16px;">{status_icon}</span>'
            f'&nbsp;&nbsp;<span style="color:#E0E0E0;">{self._html_escape(summary_line)}</span>'
        )

        # 详情体：清洗空格后直接渲染 HTML（QLabel 支持）
        detail_clean = detail_html.replace("  ●  ", "● ")
        self._result_body.setText(detail_clean)

        # 控制结果操作按钮：首次/最终打包 vs 转换缓存
        self._last_step_type = ""
        if step_index == _STEP_CACHE_CONVERT:
            self._last_step_type = "cache"
            self._btn_launch_game.setVisible(False)
            self._btn_close_game.setVisible(False)
            self._btn_open_dir.setVisible(False)
            self._btn_open_rec_dir.setVisible(True)
            self._btn_open_shk_dir.setVisible(True)
            self._btn_open_spc_dir.setVisible(True)
        elif step_index in (_STEP_FIRST_BUILD, _STEP_COLLECT_PSO, _STEP_FINAL_BUILD):
            self._last_step_type = "build"
            self._btn_launch_game.setVisible(True)
            self._btn_close_game.setVisible(True)
            self._btn_open_dir.setVisible(True)
            self._btn_open_rec_dir.setVisible(False)
            self._btn_open_shk_dir.setVisible(False)
            self._btn_open_spc_dir.setVisible(False)
        self._result_actions.setVisible(True)

        self._result_card.setVisible(True)

        # 退出 worker thread
        if self._thread:
            self._thread.quit()

    @staticmethod
    def _html_escape(text: str) -> str:
        """转义 HTML 特殊字符"""
        return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

    def _on_thread_finished(self):
        self._set_running(False)

    def _set_running(self, running: bool):
        self._running = running
        mapping = [
            (self._btn_first_build, "first"),
            (self._btn_pso_collect, "pso"),
            (self._btn_cache_convert, "cache"),
            (self._btn_final_build, "final"),
        ]
        for btn, key in mapping:
            btn.setEnabled(not running)
            s = self._BTN_STYLE[key]
            if running:
                btn.setText(s["running_text"])
            else:
                btn.setText(f"{s['icon']}{s['text']}")
        if not running and self._thread is not None:
            self._thread.wait(5000)
            self._thread = None

    # ---- 结果操作按钮辅助方法 ----

    def _make_result_action_btn(self, text: str, bg: str, hover: str, press: str) -> QPushButton:
        """创建结果卡片中的小型操作按钮"""
        btn = QPushButton(text)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.setMinimumHeight(28)
        btn.setSizePolicy(QSizePolicy.Policy.Preferred, QSizePolicy.Policy.Fixed)
        btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {bg};
                color: #FFFFFF;
                border: none;
                border-radius: 5px;
                padding: 5px 14px;
                font-size: 12px;
                font-weight: bold;
            }}
            QPushButton:hover {{
                background-color: {hover};
            }}
            QPushButton:pressed {{
                background-color: {press};
            }}
        """)
        return btn

    def _on_launch_game(self):
        """启动打包后的游戏程序"""
        proj = self._config.get_project(self._current_project_index)
        if not proj:
            return
        self._runner.set_project(self._current_project_index)
        self._runner.launch_packaged_game()

    def _on_close_game(self):
        """关闭游戏进程（按项目名匹配）"""
        proj = self._config.get_project(self._current_project_index)
        if not proj:
            return
        self._runner.set_project(self._current_project_index)
        self._runner.close_packaged_game()
        exe_name = f"{proj.name}.exe"
        try:
            result = subprocess.run(
                ["taskkill", "/F", "/IM", exe_name],
                capture_output=True, text=True, timeout=10,
            )
            if result.returncode == 0:
                self._log_output.appendPlainText(f"[关闭] 已终止 {exe_name}")
            else:
                self._log_output.appendPlainText(f"[关闭] {exe_name} 未在运行")
        except Exception as e:
            self._log_output.appendPlainText(f"[错误] 关闭游戏失败: {e}")

    def _on_open_exe_dir(self):
        """打开打包程序所在目录（用资源管理器打开文件夹）"""
        proj = self._config.get_project(self._current_project_index)
        if not proj:
            return

        output_dir = Path(proj.output_dir)
        # 候选目录：优先 exe 所在目录（Binaries/Win64），逐级降级
        candidates = [
            output_dir / "Windows",
            output_dir / "Windows" / proj.name / "Binaries" / "Win64",
            output_dir / "Windows" / proj.name,
            output_dir / "WindowsClient",
            output_dir / "WindowsClient" / proj.name / "Binaries" / "Win64",
            output_dir / "WindowsClient" / proj.name,
        ]
        for d in candidates:
            if d.is_dir():
                self._log_output.appendPlainText(f"[打开目录] {d}")
                subprocess.Popen(['explorer', str(d)])
                return

        # 降级：即使不是目录，尝试通过 exe 定位其父目录
        exe_candidates = [
            output_dir / "Windows" / f"{proj.name}.exe",
            output_dir / "Windows" / proj.name / f"{proj.name}.exe",
            output_dir / "WindowsClient" / f"{proj.name}.exe",
            output_dir / "WindowsClient" / proj.name / f"{proj.name}.exe",
        ]
        for exe_path in exe_candidates:
            if exe_path.is_file():
                parent = exe_path.parent
                self._log_output.appendPlainText(f"[打开目录] {parent}")
                subprocess.Popen(['explorer', str(parent)])
                return

        # 最终降级：输出目录本身
        if output_dir.is_dir():
            self._log_output.appendPlainText(f"[打开目录] {output_dir}")
            subprocess.Popen(['explorer', str(output_dir)])
        else:
            self._log_output.appendPlainText("[警告] 输出目录不存在，无法打开")

    def _on_open_cache_dir(self, dir_type: str):
        """打开转换缓存对应目录：rec / shk / spc"""
        proj = self._config.get_project(self._current_project_index)
        if not proj:
            return

        if dir_type == "rec":
            target = Path(proj.resolve_rec_source_dir())
            label = "REC源文件"
        elif dir_type == "shk":
            target = Path(proj.resolve_shk_source_dir())
            label = "SHK源文件"
        else:  # "spc"
            target = Path(proj.resolve_spc_target_dir())
            label = "目标SPC"

        if target.is_dir():
            self._log_output.appendPlainText(f"[打开目录] {label}: {target}")
            os.startfile(str(target))
        else:
            self._log_output.appendPlainText(f"[警告] {label} 目录不存在: {target}")

    # ---- 详情窗口辅助 ----

    def _append_detail(self, text: str, color: str = "#CCCCCC"):
        """向执行详情窗口追加一条 HTML 条目"""
        self._log_output.appendHtml(f'<span style="color:{color}">{text}</span>')

