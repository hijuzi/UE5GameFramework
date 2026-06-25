"""
LogIssueWidget - 日志归档标签页
按阶段（首次打包 / 转换缓存 / 最终打包）查看阶段指令和日志
支持按日志级别（Log / Warning / Error）多选过滤
"""

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPlainTextEdit,
    QCheckBox, QGroupBox, QLabel, QComboBox, QSizePolicy,
)
from PySide6.QtGui import QTextCharFormat, QColor, QFont
from PySide6.QtCore import Qt


# 四个阶段与 Step 索引的映射
PHASE_STEPS = {
    "PSO 收集": 3,
    "首次打包": 2,
    "转换缓存": 6,
    "最终打包": 8,
}

# 日志级别映射：checkbox 标签 → 内部 level 集合
LEVEL_GROUPS = {
    "Log":     ("INFO", "SUCCESS"),   # Log 包含 INFO + SUCCESS
    "Warning": ("WARNING",),
    "Error":   ("ERROR",),
}

LOG_COLORS = {
    "INFO":    QColor("#CCCCCC"),
    "SUCCESS": QColor("#4ECB71"),
    "WARNING": QColor("#F0C040"),
    "ERROR":   QColor("#E0556A"),
}

# 指令输出区域颜色
CMD_BG = "#161821"
CMD_BORDER = "#3D3D3D"
CMD_PATH = "#F0C040"


class LogIssueWidget(QWidget):
    """日志归档标签页 — 按阶段查看指令 + 按级别过滤查看日志"""

    def __init__(self, parent=None):
        super().__init__(parent)

        # 每个阶段独立存储日志: step_index -> [(level, message), ...]
        self._phase_buffers: dict[int, list[tuple[str, str]]] = {
            2: [],  # 首次打包
            3: [],  # PSO 收集
            6: [],  # 转换缓存
            8: [],  # 最终打包
        }

        # 每个阶段存储完整的指令文本
        self._phase_commands: dict[int, str] = {
            2: "",
            3: "",
            6: "",
            8: "",
        }

        # UI 状态
        self._current_phase_step: int = 2        # 当前选中的阶段
        self._active_levels: set[str] = {"INFO", "SUCCESS", "WARNING", "ERROR"}

        self._setup_ui()
        self._setup_style()

    # ---- 公共接口 ----

    def append_phase_log(self, step_index: int, level: str, message: str):
        """追加日志到指定阶段的缓冲区"""
        if step_index not in self._phase_buffers:
            return
        self._phase_buffers[step_index].append((level, message))

    def set_phase_command(self, step_index: int, cmd: str):
        """设置指定阶段的完整指令文本"""
        if step_index in self._phase_commands:
            self._phase_commands[step_index] = cmd
            # 如果当前正在查看该阶段，立即刷新指令面板
            if step_index == self._current_phase_step:
                self._redraw_command()

    def refresh_display(self):
        """根据当前选中的阶段和过滤器刷新日志显示"""
        self._redraw_log()

    # ---- UI 构建 ----

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(6)

        # ---- 控制栏 ----
        control_bar = QHBoxLayout()
        control_bar.setSpacing(12)

        # 阶段选择
        phase_label = QLabel("阶段选择:")
        phase_label.setStyleSheet("color: #CCCCCC; font-weight: bold;")
        control_bar.addWidget(phase_label)

        self._phase_combo = QComboBox()
        self._phase_combo.setMinimumWidth(140)
        self._phase_combo.addItems(["PSO 收集", "首次打包", "转换缓存", "最终打包"])
        self._phase_combo.currentTextChanged.connect(self._on_phase_changed)
        control_bar.addWidget(self._phase_combo)

        control_bar.addSpacing(20)

        # 日志级别过滤（多选 checkbox）
        filter_label = QLabel("级别过滤:")
        filter_label.setStyleSheet("color: #CCCCCC; font-weight: bold;")
        control_bar.addWidget(filter_label)

        self._chk_log = QCheckBox("Log")
        self._chk_log.setChecked(True)
        self._chk_log.toggled.connect(self._on_filter_changed)
        control_bar.addWidget(self._chk_log)

        self._chk_warning = QCheckBox("Warning")
        self._chk_warning.setChecked(True)
        self._chk_warning.toggled.connect(self._on_filter_changed)
        control_bar.addWidget(self._chk_warning)

        self._chk_error = QCheckBox("Error")
        self._chk_error.setChecked(True)
        self._chk_error.toggled.connect(self._on_filter_changed)
        control_bar.addWidget(self._chk_error)

        # 统计信息
        control_bar.addStretch()
        self._lbl_stats = QLabel("共 0 条日志")
        self._lbl_stats.setStyleSheet("color: #888888; font-size: 11px;")
        control_bar.addWidget(self._lbl_stats)

        layout.addLayout(control_bar)

        # ---- 指令输出区域 ----
        cmd_group = QGroupBox("指令输出")
        cmd_layout = QVBoxLayout(cmd_group)
        cmd_layout.setContentsMargins(4, 8, 4, 4)

        self._cmd_view = QPlainTextEdit()
        self._cmd_view.setReadOnly(True)
        self._cmd_view.setMaximumBlockCount(500)
        self._cmd_view.setSizePolicy(
            QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred
        )
        self._cmd_view.setFixedHeight(120)   # 固定高度，保持紧凑
        font_cmd = QFont("Cascadia Code", 10)
        font_cmd.setStyleHint(QFont.StyleHint.Monospace)
        self._cmd_view.setFont(font_cmd)
        self._cmd_view.setPlaceholderText("（等待阶段执行指令...）")
        cmd_layout.addWidget(self._cmd_view)

        layout.addWidget(cmd_group)

        # ---- 日志显示区域 ----
        log_group = QGroupBox("日志详情")
        log_layout = QVBoxLayout(log_group)
        log_layout.setContentsMargins(4, 8, 4, 4)

        self._log_view = QPlainTextEdit()
        self._log_view.setReadOnly(True)
        self._log_view.setMaximumBlockCount(5000)
        font_log = QFont("Cascadia Code", 10)
        font_log.setStyleHint(QFont.StyleHint.Monospace)
        self._log_view.setFont(font_log)
        log_layout.addWidget(self._log_view)

        layout.addWidget(log_group, stretch=1)

        # 初始化显示
        self._redraw_command()
        self._redraw_log()

    # ---- 事件处理 ----

    def _on_phase_changed(self, phase_name: str):
        self._current_phase_step = PHASE_STEPS.get(phase_name, 2)
        self._redraw_command()
        self._redraw_log()

    def _on_filter_changed(self):
        # 收集当前激活的日志级别
        active = set()
        if self._chk_log.isChecked():
            active.update(LEVEL_GROUPS["Log"])
        if self._chk_warning.isChecked():
            active.update(LEVEL_GROUPS["Warning"])
        if self._chk_error.isChecked():
            active.update(LEVEL_GROUPS["Error"])
        self._active_levels = active
        self._redraw_log()

    # ---- 指令渲染 ----

    def _redraw_command(self):
        """渲染当前阶段的完整指令（高亮格式化）"""
        step = self._current_phase_step
        cmd = self._phase_commands.get(step, "")

        self._cmd_view.clear()

        if not cmd:
            self._cmd_view.setPlaceholderText("（等待阶段执行指令...）")
            return

        # 格式化：将长命令行按关键标志位换行缩进
        formatted = self._format_command(cmd)

        self._cmd_view.appendPlainText(formatted)

    def _format_command(self, cmd: str) -> str:
        """将命令行格式化为多行缩进布局

        规则：
        - 首次/最终打包（RunUAT.bat）：EXE 独占第一行，后续每个 -flag 独立一行缩进
        - 转换缓存（ShaderPipelineCacheTools）：EXE + uproject 第一行，后续参数独立行缩进
        """
        # 去掉首尾空白
        cmd = cmd.strip()

        # 检测是 UAT 打包还是 ShaderPipelineCacheTools
        if "ShaderPipelineCacheTools" in cmd:
            return self._format_expand_cmd(cmd)
        else:
            return self._format_build_cmd(cmd)

    @staticmethod
    def _format_build_cmd(cmd: str) -> str:
        """格式化 UAT BuildCookRun 打包指令

        输入: RunUAT.bat BuildCookRun -project=... -platform=Win64 ...
        输出: 按行缩进排列的参数列表
        """
        # 找到 " BuildCookRun" 分割前缀和参数
        parts = cmd.split(" BuildCookRun", 1)
        if len(parts) != 2:
            return cmd  # 格式不匹配，原样返回

        prefix = parts[0].strip() + " BuildCookRun"
        args_str = parts[1].strip()

        # 用空格分割参数，并将 -flag 组合
        tokens = args_str.split()
        lines = [prefix]

        # 特殊：将 -build -cook -stage -pak -archive 合并为一行
        group_flags = {"-build", "-cook", "-stage", "-pak", "-archive"}

        current_line_parts = []
        for tok in tokens:
            if tok.startswith("-") or tok.startswith('"'):
                # 遇到 flag / 带引号值，先输出前面积累的行
                current_flag = tok.split("=")[0].split(":")[0] if "=" in tok else tok.strip('"')
                pure_flag = current_flag.lstrip("-").lstrip('"')
                # 检查是否属于 group_flags（纯标志无等号无引号）
                if tok in group_flags or (tok.startswith("-") and tok.lstrip("-") in {"build", "cook", "stage", "pak", "archive"}):
                    current_line_parts.append(tok)
                else:
                    if current_line_parts:
                        lines.append("    " + " ".join(current_line_parts))
                        current_line_parts = []
                    if tok.startswith('"') and not "=" in tok:
                        # 纯路径值
                        lines.append(f"    {tok}")
                    else:
                        lines.append(f"    {tok}")
            else:
                current_line_parts.append(tok)

        if current_line_parts:
            lines.append("    " + " ".join(current_line_parts))

        return "\n".join(lines)

    @staticmethod
    def _format_expand_cmd(cmd: str) -> str:
        """格式化 ShaderPipelineCacheTools expand 指令

        输入: UnrealEditor-Cmd.exe project.uproject -run=ShaderPipelineCacheTools expand ...
        输出: 按行缩进排列的参数列表
        """
        tokens = []
        current = ""
        in_quotes = False

        for ch in cmd:
            if ch == '"':
                in_quotes = not in_quotes
                current += ch
            elif ch == ' ' and not in_quotes:
                if current.strip():
                    tokens.append(current.strip())
                current = ""
            else:
                current += ch
        if current.strip():
            tokens.append(current.strip())

        if len(tokens) < 3:
            return cmd

        # 第一行：Editor-Cmd.exe
        lines = [tokens[0]]
        # 第二行：uproject 路径
        if tokens[1].startswith('"'):
            lines.append(f"    {tokens[1]}")
        else:
            lines.append(f"    {tokens[1]}")

        # 后续参数每行一个
        for tok in tokens[2:]:
            if tok == "-NullRHI" or tok == "-unattended":
                # 合并简单标志到一行
                continue
            lines.append(f"    {tok}")

        # NullRHI + unattended 合并到最后
        if "-NullRHI" in tokens and "-unattended" in tokens:
            lines.append("    -NullRHI -unattended")
        elif "-NullRHI" in tokens:
            lines.append("    -NullRHI")
        elif "-unattended" in tokens:
            lines.append("    -unattended")

        return "\n".join(lines)

    # ---- 日志渲染 ----

    def _redraw_log(self):
        """根据当前选项重新渲染日志"""
        step = self._current_phase_step
        logs = self._phase_buffers.get(step, [])

        # 过滤
        filtered = [(level, msg) for level, msg in logs if level in self._active_levels]

        self._log_view.clear()

        # 构建格式
        formats = {
            level: self._make_format(color)
            for level, color in LOG_COLORS.items()
        }

        for level, msg in filtered:
            fmt = formats.get(level, formats["INFO"])
            self._log_view.setCurrentCharFormat(fmt)
            self._log_view.appendPlainText(msg)

        self._lbl_stats.setText(
            f"共 {len(filtered)} 条日志"
            + (f" (过滤前 {len(logs)} 条)" if len(filtered) != len(logs) else "")
        )

    @staticmethod
    def _make_format(color: QColor) -> QTextCharFormat:
        fmt = QTextCharFormat()
        fmt.setForeground(color)
        return fmt

    # ---- 样式 ----

    def _setup_style(self):
        self._cmd_view.setStyleSheet(f"""
            QPlainTextEdit {{
                background-color: {CMD_BG};
                border: 1px solid {CMD_BORDER};
                border-radius: 4px;
                color: {CMD_PATH};
                padding: 4px 6px;
            }}
        """)
        self._log_view.setStyleSheet("""
            QPlainTextEdit {
                background-color: #1A1A1A;
                border: 1px solid #3D3D3D;
                border-radius: 4px;
                color: #CCCCCC;
            }
        """)
