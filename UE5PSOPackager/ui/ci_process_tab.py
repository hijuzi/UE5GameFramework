"""
CIProcessTab - CI流程配置标签页
支持：PSOCache刷新资产（Commandlet）+ 打包流程集成
左侧：参数配置区 / 右侧：执行详情 + 执行结果

===== 设计约束（与 参数管理 保持一致）=====
【主要约束】本标签页与 build_params_tab 在功能上和设计排版上保持基本一致。
【详细约束1】所有涉及路径的参数行，均通过 _attach_path_indicators 挂载路径有效性指示器（✅/❌）。
            指标器刷新由 _fill_param_rows / _clear_param_rows 自动完成，无需各 _populate_* 方法单独调用。
【详细约束2】本标签页中，任何执行操作（如点击"PSOCache刷新资产"）必须先检查编辑器是否打开。
            若打开 → 弹窗询问"是否关闭编辑器后继续"，用户确认后关闭编辑器再执行，用户取消则退出。
【详细约束3】参数管理与CI流程配置中，任何执行操作均需向日志输出区输出信息：
            - 执行详情（_log_output）：显示执行过程的主要信息（启动、进度、中间日志）
            - 执行结果（_result_card / _result_body）：执行完成后显示最终汇总结果（成功/失败/耗时/关键统计）
【详细约束4】「执行全部CI流程」（工作流 Step 2）将 CI 流程配置中所有项依次执行，
            每个 CI 子步骤（如 PSOCache刷新资产）在工作流详情中需独立显示是否执行成功、执行耗时。
"""
import subprocess
from pathlib import Path
from datetime import datetime

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QLineEdit, QPushButton, QLabel, QSplitter,
    QComboBox, QFileDialog, QScrollArea, QCheckBox,
    QPlainTextEdit, QSizePolicy, QFrame, QMessageBox,
)
from PySide6.QtCore import Qt, Signal, QThread

from config_manager import ConfigManager
from ui.config_tab import (
    _create_param_table, _fill_param_rows, _clear_param_rows,
    _attach_path_indicators, NoScrollComboBox,
)

# ---- CI 流程参数定义 ----

_PSOCACHE_REFRESH_PARAMS = [
    ("编辑器路径", "-UnrealEditor-Cmd.exe", "editor_cmd", "readonly", None),
    ("项目路径", "-project", "project", "readonly", None),
    ("运行命令", "-run=", "run_cmd", "readonly_text", None),
    ("资产包路径", "-PackagePath=", "package_path", "readonly", None),  # 显示完整文件系统路径 + 📂 打开文件夹
    ("NullRHI", "-NullRHI", "null_rhi", "check", None),
    ("无交互", "-unattended", "unattended", "check", None),
]

# 模拟 ParamRow 兼容 _fill_param_rows 的数据结构
class _FakeParamRow:
    def __init__(self, desc, flag, value_key, widget_type, choices):
        self.desc = desc
        self.flag = flag
        self.value_key = value_key
        self.widget_type = widget_type
        self.choices = choices

_PSO_PARAM_DEFS = [_FakeParamRow(*p) for p in _PSOCACHE_REFRESH_PARAMS]


class _WorkerThread(QThread):
    """简单的 Worker 线程：只执行 target"""
    def __init__(self, target, parent=None):
        super().__init__(parent)
        self._target = target

    def run(self):
        self._target()


class CIProcessTab(QWidget):
    """CI流程配置标签页"""

    # ---- 信号（用于跨线程安全通信 / 转发到主窗口日志） ----
    log_signal = Signal(str, str)                       # (level, message) → 日志输出 tab
    result_ready = Signal(bool, float, int, int)        # (success, elapsed, detail_count, total_lines) → 结果卡片

    def __init__(self, config: ConfigManager, parent=None):
        super().__init__(parent)
        self._config = config
        self._current_project_index: int = -1
        self._current_ue5_version: str = ""

        # 参数行
        self._pso_refresh_rows: dict[str, tuple] = {}

        # 路径有效性指示器
        self._pso_indicators: dict = {}

        # 执行状态
        self._running = False
        self._thread: QThread | None = None

        # 连接结果信号到显示槽
        self.result_ready.connect(self._show_result)

        self._setup_ui()

    # ---- UI 构建 ----

    def _setup_ui(self):
        root = QHBoxLayout(self)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(0)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.setHandleWidth(3)
        splitter.setStyleSheet(
            "QSplitter::handle { background-color: #3D3D3D; border-radius: 1px; }"
        )

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

        # --- PSOCache刷新资产 ---
        psog = QGroupBox("  PSOCache刷新资产")
        psog.setStyleSheet(gb_style)
        psol = QVBoxLayout(psog)
        psol.setSpacing(2)
        self._pso_refresh_rows = _create_param_table(_PSO_PARAM_DEFS, self, font_scale=1.2)
        for widget in self._pso_refresh_rows.values():
            psol.addWidget(widget[0])

        # 路径有效性指示器 (编辑器路径、项目路径、资产包路径)
        self._pso_indicators = _attach_path_indicators(
            self._pso_refresh_rows, {"editor_cmd", "project", "package_path"}
        )

        # 额外说明文本
        hint = QLabel(
            "通过 Commandlet 扫描项目中所有 Material 和 NiagaraSystem 资产，"
            "刷新 PSOCacheAssetList DataAsset，供 PSO 预编译打包流程使用。"
        )
        hint.setWordWrap(True)
        hint.setStyleSheet("color: #888888; font-size: 11px; padding: 6px 14px 0 14px;")
        psol.addWidget(hint)
        cl.addWidget(psog)

        cl.addStretch()
        scroll.setWidget(content)
        left_layout.addWidget(scroll)
        splitter.addWidget(left)

        # ===================================================================
        # 右侧：操作区 (stretch=3)
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

        self._btn_execute = QPushButton("执行 PSOCache刷新资产")
        self._btn_execute.setCursor(Qt.CursorShape.PointingHandCursor)
        self._btn_execute.setMinimumHeight(38)
        self._btn_execute.setStyleSheet("""
            QPushButton {
                background-color: #0078D4;
                color: #FFFFFF;
                border: none;
                border-radius: 6px;
                padding: 8px 20px;
                font-size: 13px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #1A8CE8; }
            QPushButton:pressed { background-color: #005A9E; }
            QPushButton:disabled { background-color: #2D2D2D; color: #666666; }
        """)
        self._btn_execute.clicked.connect(self._on_execute)
        btn_card_layout.addWidget(self._btn_execute)

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
        self._log_output.setPlaceholderText("点击上方按钮执行 CI 任务，此处显示实时输出…")
        self._log_output.setSizePolicy(
            QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding
        )
        detail_layout.addWidget(self._log_output, 1)
        right_layout.addWidget(detail_card, 1)

        # ---- 执行结果卡片（完成后显示） ----
        self._result_card = QGroupBox("  执行结果")
        self._result_card.setStyleSheet(gb_style)
        result_layout = QVBoxLayout(self._result_card)
        result_layout.setContentsMargins(12, 18, 12, 14)
        result_layout.setSpacing(10)

        self._result_header = QLabel()
        self._result_header.setWordWrap(True)
        self._result_header.setStyleSheet(
            "font-size: 14px; font-weight: bold; padding: 0; background: transparent;"
        )
        result_layout.addWidget(self._result_header)

        sep = QLabel()
        sep.setFixedHeight(1)
        sep.setStyleSheet("background-color: #3A3A3A;")
        result_layout.addWidget(sep)

        self._result_body = QLabel()
        self._result_body.setWordWrap(True)
        self._result_body.setStyleSheet(
            "font-size: 12px; padding: 0; background: transparent; line-height: 1.6;"
        )
        result_layout.addWidget(self._result_body)

        self._result_card.setMinimumHeight(160)
        self._result_card.setVisible(False)
        right_layout.addWidget(self._result_card)

        splitter.addWidget(right)
        splitter.setStretchFactor(0, 5)
        splitter.setStretchFactor(1, 3)
        root.addWidget(splitter)

    # ---- 刷新参数 ----

    def refresh(self, project_index: int, ue5_version: str):
        """根据当前选中的项目和 UE 版本刷新参数"""
        self._current_project_index = project_index
        self._current_ue5_version = ue5_version
        self._populate_pso_refresh_params()

    def _populate_pso_refresh_params(self):
        """填充 PSOCache刷新资产 参数"""
        proj = self._config.get_project(self._current_project_index)
        ver = self._current_ue5_version
        ue = self._config.ue5_versions.get(ver) if ver else None
        rows = self._pso_refresh_rows
        indicators = self._pso_indicators

        if not proj or not ue:
            _clear_param_rows(rows, indicators)
            return

        # 计算资产包在文件系统中的完整路径
        # UE5 插件内容挂载规则: /PluginName/ -> Plugins/PluginName/Content/
        content_path = "/PSOCacheSystem/DA/DA_PSOCacheAssetList"
        fs_package_path = content_path  # 降级默认值
        if proj and proj.uproject_file:
            proj_dir = Path(proj.uproject_file).parent
            parts = content_path.strip("/").split("/", 1)
            if len(parts) >= 1:
                plugin_name = parts[0]  # "PSOCacheSystem"
                rel_path = parts[1] if len(parts) > 1 else ""  # "DA/DA_PSOCacheAssetList"
                plugin_content = proj_dir / "Plugins" / plugin_name / "Content" / rel_path
                candidate = str(plugin_content.with_suffix(".uasset")).replace("\\", "/")
                if Path(candidate).exists():
                    fs_package_path = candidate
                else:
                    # 降级：尝试项目 Content 目录
                    fallback = str(proj_dir / "Content" / rel_path).replace("\\", "/") + ".uasset"
                    fs_package_path = fallback if Path(fallback).exists() else content_path

        defaults = {
            "editor_cmd":  ue.editor_cmd_path or "",
            "project":     proj.uproject_file or "",
            "run_cmd":     "RefreshPSOCacheAssetList",
            "package_path": fs_package_path,
            "null_rhi":    True,
            "unattended":  True,
        }
        _fill_param_rows(rows, defaults, indicators)

    # ---- 收集 UI 参数 ----

    def _collect_ui_params(self, rows: dict) -> dict:
        """从 UI 参数行收集全部值"""
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

    # ---- 执行 ----

    @staticmethod
    def _is_editor_running() -> bool:
        """检测 UnrealEditor.exe 或 UnrealEditor-Cmd.exe 是否正在运行"""
        try:
            for name in ("UnrealEditor.exe", "UnrealEditor-Cmd.exe"):
                result = subprocess.run(
                    ["tasklist", "/FI", f"IMAGENAME eq {name}"],
                    capture_output=True, text=True, timeout=5,
                )
                if name in result.stdout:
                    return True
        except Exception:
            pass
        return False

    @staticmethod
    def _kill_editor() -> bool:
        """强制关闭 UnrealEditor.exe 和 UnrealEditor-Cmd.exe 进程"""
        try:
            for name in ("UnrealEditor.exe", "UnrealEditor-Cmd.exe"):
                subprocess.run(
                    ["taskkill", "/F", "/IM", name],
                    capture_output=True, text=True, timeout=15,
                )
            return True
        except Exception:
            return False

    def _on_execute(self):
        """执行 PSOCache刷新资产 命令"""
        if self._running:
            return

        #【详细约束2】编辑器运行检测（必须在参数校验之前，弹窗在清空日志之前）
        editor_was_running = self._is_editor_running()
        if editor_was_running:
            reply = QMessageBox.question(
                self,
                "编辑器正在运行",
                "检测到 UnrealEditor 正在运行，Commandlet 无法同时操作同一项目。\n\n"
                "是否关闭编辑器并继续执行？",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if reply != QMessageBox.StandardButton.Yes:
                return
            # 用户选择关闭编辑器
            if not self._kill_editor():
                QMessageBox.warning(self, "关闭失败", "无法关闭编辑器进程，请手动关闭后重试。")
                return

        # 清空日志并写入标题行（必须先于校验，确保错误信息显示在干净窗口）
        self._log_output.clear()
        self._result_card.setVisible(False)
        ts = datetime.now().strftime("%H:%M:%S")
        self._log_output.appendPlainText(f"══ PSOCache刷新资产 ══")

        proj = self._config.get_project(self._current_project_index)
        ver = self._current_ue5_version
        ue = self._config.ue5_versions.get(ver) if ver else None

        if not proj:
            self._log_output.appendPlainText(f"[{ts}] [错误] 未选择项目")
            return
        if not ue or not ue.editor_cmd_path:
            self._log_output.appendPlainText(f"[{ts}] [错误] 未配置 UE Editor-Cmd 路径")
            return

        # 收集参数
        params = self._collect_ui_params(self._pso_refresh_rows)
        editor_cmd = params.get("editor_cmd", "")
        uproject = params.get("project", "")
        package_path_fs = params.get("package_path", "/PSOCacheSystem/DA/DA_PSOCacheAssetList")
        use_null_rhi = params.get("null_rhi", True)
        use_unattended = params.get("unattended", True)

        # 将文件系统路径还原为 UE Content 路径（供 Commandlet 使用）
        # 插件资产: .../Plugins/PluginName/Content/... → /PluginName/...
        # 项目资产: .../Content/...                     → /...
        package_path_content = package_path_fs
        if "/Plugins/" in package_path_fs and "/Content/" in package_path_fs:
            _, rest = package_path_fs.split("/Plugins/", 1)
            plugin_name, content_rel = rest.split("/Content/", 1)
            package_path_content = "/" + plugin_name + "/" + content_rel.replace(".uasset", "")
        elif "/Content/" in package_path_fs:
            package_path_content = "/" + package_path_fs.split("/Content/", 1)[1].replace(".uasset", "")

        # 构建命令
        extra_flags = []
        if use_null_rhi:
            extra_flags.append("-NullRHI")
        if use_unattended:
            extra_flags.append("-unattended")

        extra_str = " ".join(extra_flags)
        cmd = (
            f'"{editor_cmd}"'
            f' "{uproject}"'
            f' -run=RefreshPSOCacheAssetList'
            f' -PackagePath="{package_path_content}"'
            f' {extra_str}'
        ).strip()

        # 追加执行详情信息
        if editor_was_running:
            self._log_output.appendPlainText(f"[{ts}] UnrealEditor 已自动关闭")
        self._log_output.appendPlainText(f"[{ts}] 正在执行 Commandlet…")
        self._log_output.appendPlainText(f"")
        self._log_output.appendPlainText(f">> {cmd}")
        self._log_output.appendPlainText(f"")

        self._set_running(True)

        # 在 Worker 线程中执行
        self._thread = _WorkerThread(lambda: self._run_command(cmd))
        self._thread.finished.connect(self._on_thread_finished)
        self._thread.started.connect(lambda: None)  # 确保 finished 触发
        self._thread.start()

    def _run_command(self, cmd: str):
        """在 worker 线程中执行命令，捕获输出并发送到 UI"""
        start_time = datetime.now()

        try:
            proc = subprocess.Popen(
                cmd,
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
            )

            output_lines = []
            for line in proc.stdout:
                line = line.rstrip("\n\r")
                if line:
                    output_lines.append(line)
                    lo = line.lower()
                    if "error" in lo or "fail" in lo:
                        self._emit_log("ERROR", line)
                    elif "warn" in lo:
                        self._emit_log("WARNING", line)
                    elif "success" in lo or "完成" in line or "刷新" in line:
                        self._emit_log("SUCCESS", line)
                    else:
                        # 普通 INFO 行：只转发到日志输出 tab，不写入执行详情
                        self.log_signal.emit("INFO", line)

            proc.wait()
            rc = proc.returncode
            elapsed = (datetime.now() - start_time).total_seconds()

            if rc == 0:
                self._emit_log("SUCCESS", f"")
                self._emit_log("SUCCESS", f"命令执行成功 (返回码: 0, 耗时: {self._fmt_elapsed(elapsed)})")
                self._signal_result(True, elapsed, output_lines)
            else:
                self._emit_log("ERROR", f"")
                self._emit_log("ERROR", f"命令执行失败 (返回码: {rc}, 耗时: {self._fmt_elapsed(elapsed)})")
                self._signal_result(False, elapsed, output_lines)

        except Exception as e:
            elapsed = (datetime.now() - start_time).total_seconds()
            self._emit_log("ERROR", f"命令执行异常: {e}")
            self._signal_result(False, elapsed, [str(e)])

    def _emit_log(self, level: str, message: str):
        """线程安全地发射日志到「执行详情」+ 转发到主窗口「日志输出」tab"""
        # ① 转发到主窗口「日志输出」tab（全部级别）
        self.log_signal.emit(level, message)

        # ② 写入本标签页「执行详情」区域
        timestamp = datetime.now().strftime("%H:%M:%S")
        color = {
            "SUCCESS": "#4ECB71",
            "ERROR": "#E0556A",
            "WARNING": "#D4A843",
            "INFO": "#AAAAAA",
        }.get(level, "#CCCCCC")
        html = (
            f'<span style="color:{color}">'
            f'[{timestamp}] {self._html_escape(message)}'
            f'</span>'
        )
        # 通过 invokeMethod 确保主线程更新 UI
        from PySide6.QtCore import QMetaObject, Qt as QtCore_Qt, Q_ARG
        QMetaObject.invokeMethod(
            self._log_output,
            "appendHtml",
            QtCore_Qt.ConnectionType.QueuedConnection,
            Q_ARG(str, html),
        )

    def _signal_result(self, success: bool, elapsed: float, output_lines: list[str]):
        """线程安全地发射执行结果（通过 Qt 信号自动跨线程）"""
        detail_count = len([l for l in output_lines if "Material" in l or "Niagara" in l])
        self.result_ready.emit(success, elapsed, detail_count, len(output_lines))

    def _show_result(self, success: bool, elapsed: float, detail_count: int, total_lines: int):
        """在主线程中显示执行结果（由 result_ready 信号驱动）"""
        is_success = success
        status_color = "#4ECB71" if is_success else "#E0556A"
        status_icon = "✓" if is_success else "✗"
        summary = "成功" if is_success else "失败"

        self._result_header.setText(
            f'<span style="color:{status_color}; font-size:16px;">{status_icon}</span>'
            f'&nbsp;&nbsp;'
            f'<span style="color:#E0E0E0;">'
            f'PSOCache刷新资产 — {summary} ({self._fmt_elapsed(elapsed)})'
            f'</span>'
        )

        body_parts = []
        if is_success:
            body_parts.append(
                f'<span style="color:#4ECB71;">●</span> '
                f'<span style="color:#CCCCCC;">Commandlet 执行成功</span>'
            )
            body_parts.append(
                f'<span style="color:#4ECB71;">●</span> '
                f'<span style="color:#CCCCCC;">全项目 Material 和 NiagaraSystem 资产已刷新到'
                f' /PSOCacheSystem/DA/DA_PSOCacheAssetList</span>'
            )
            body_parts.append(
                f'<span style="color:#4ECB71;">●</span> '
                f'<span style="color:#CCCCCC;">共输出 {total_lines} 行日志</span>'
            )
        else:
            body_parts.append(
                f'<span style="color:#E0556A;">●</span> '
                f'<span style="color:#CCCCCC;">执行耗时: {self._fmt_elapsed(elapsed)}</span>'
            )
            body_parts.append(
                f'<span style="color:#E0556A;">●</span> '
                f'<span style="color:#CCCCCC;">请检查 UE Editor-Cmd 路径和项目配置是否正确</span>'
            )
            body_parts.append(
                f'<span style="color:#E0556A;">●</span> '
                f'<span style="color:#CCCCCC;">共输出 {total_lines} 行日志，请查看上方执行详情</span>'
            )

        self._result_body.setText("<br>".join(body_parts))
        self._result_card.setVisible(True)

    def _on_thread_finished(self):
        """Worker 线程完成"""
        self._set_running(False)

    def _set_running(self, running: bool):
        self._running = running
        if running:
            self._btn_execute.setEnabled(False)
            self._btn_execute.setText("执行中...")
        else:
            self._btn_execute.setEnabled(True)
            self._btn_execute.setText("执行 PSOCache刷新资产")
            if self._thread is not None:
                self._thread.wait(5000)
                self._thread = None

    # ---- 工具方法 ----

    @staticmethod
    def _html_escape(text: str) -> str:
        return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

    @staticmethod
    def _fmt_elapsed(seconds: float) -> str:
        if seconds < 60:
            return f"{seconds:.1f}s"
        elif seconds < 3600:
            m, s = divmod(seconds, 60)
            return f"{int(m)}m {int(s)}s"
        else:
            h, r = divmod(seconds, 3600)
            m, s = divmod(r, 60)
            return f"{int(h)}h {int(m)}m {int(s)}s"
