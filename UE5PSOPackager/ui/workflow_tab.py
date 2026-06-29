"""
WorkflowTab - 工作流执行标签页
步骤清单 + 执行按钮组 + 进度条 + Step 4 操作面板 + Step 10 面板 + 右侧步骤详情
"""
import subprocess
from pathlib import Path

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QListWidget,
    QListWidgetItem, QPushButton, QProgressBar,
    QLabel, QGroupBox, QSplitter, QScrollArea, QFrame, QSizePolicy,
    QMessageBox, QDialog, QDialogButtonBox,
)
from PySide6.QtCore import Qt, Signal, QThread, QTimer
from PySide6.QtGui import QFont

from step_definitions import PSO_WORKFLOW_STEPS, StepStatus
from step_runner import StepRunner


class _WorkerThread(QThread):
    """简单的 Worker 线程：只执行 target，runner 始终留在主线程，避免 moveToThread 线程亲和性问题。"""
    def __init__(self, target, parent=None):
        super().__init__(parent)
        self._target = target

    def run(self):
        self._target()

STATUS_ICONS = {
    StepStatus.PENDING.value: "○",
    StepStatus.RUNNING.value: "▶",
    StepStatus.SUCCESS.value: "✓",
    StepStatus.FAILED.value:  "✗",
    StepStatus.SKIPPED.value: "⊙",
}

STATUS_COLORS = {
    StepStatus.PENDING.value: "#888888",
    StepStatus.RUNNING.value: "#4FC3F7",
    StepStatus.SUCCESS.value: "#4ECB71",
    StepStatus.FAILED.value:  "#E0556A",
    StepStatus.SKIPPED.value: "#666666",
}

STATUS_BORDER = {
    StepStatus.PENDING.value: "#888888",
    StepStatus.RUNNING.value: "#4FC3F7",
    StepStatus.SUCCESS.value: "#4ECB71",
    StepStatus.FAILED.value:  "#E0556A",
    StepStatus.SKIPPED.value: "#444444",
}


class StepResultCard(QFrame):
    """单个步骤的结果卡片 — 支持摘要 + 可折叠详情"""

    def __init__(self, step, parent=None):
        super().__init__(parent)
        self._step = step
        self._status = StepStatus.PENDING.value
        self._result_text = ""
        self._detail_lines: list[str] = []
        self._detail_expanded = False
        self._setup_ui()

    def _setup_ui(self):
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self.setFrameShape(QFrame.Shape.StyledPanel)
        self.setStyleSheet(self._card_style(STATUS_BORDER[StepStatus.PENDING.value]))

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 8, 10, 8)
        layout.setSpacing(4)

        # 顶部行：状态图标 + 步骤名
        top = QHBoxLayout()
        top.setSpacing(6)

        self._lbl_icon = QLabel(STATUS_ICONS[StepStatus.PENDING.value])
        self._lbl_icon.setFixedWidth(20)
        self._lbl_icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._lbl_icon.setStyleSheet(f"color: {STATUS_COLORS[StepStatus.PENDING.value]}; font-size: 14px; font-weight: bold;")
        top.addWidget(self._lbl_icon)

        self._lbl_name = QLabel(f"Step {self._step.index}: {self._step.name}")
        self._lbl_name.setStyleSheet("font-weight: bold; font-size: 13px; color: #E0E0E0;")
        top.addWidget(self._lbl_name)

        top.addStretch()

        self._lbl_time = QLabel(self._step.estimated_time)
        self._lbl_time.setStyleSheet("color: #888888; font-size: 11px;")
        top.addWidget(self._lbl_time)

        layout.addLayout(top)

        # 描述行
        self._lbl_desc = QLabel(self._step.description)
        self._lbl_desc.setWordWrap(True)
        self._lbl_desc.setStyleSheet("color: #999999; font-size: 11px; padding-left: 26px;")
        layout.addWidget(self._lbl_desc)

        # 摘要行（执行完成后显示）
        self._lbl_summary = QLabel("")
        self._lbl_summary.setWordWrap(True)
        self._lbl_summary.setVisible(False)
        self._lbl_summary.setStyleSheet("color: #4ECB71; font-size: 12px; padding-left: 26px; padding-top: 4px; font-weight: bold;")
        layout.addWidget(self._lbl_summary)

        # 详情容器
        self._detail_container = QWidget()
        self._detail_container.setVisible(False)
        detail_container_layout = QVBoxLayout(self._detail_container)
        detail_container_layout.setContentsMargins(26, 0, 0, 0)
        detail_container_layout.setSpacing(1)

        self._lbl_details = QLabel("")
        self._lbl_details.setWordWrap(True)
        self._lbl_details.setStyleSheet("color: #AAAAAA; font-size: 11px; line-height: 1.5;")
        self._lbl_details.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        detail_container_layout.addWidget(self._lbl_details)

        layout.addWidget(self._detail_container)

        # 展开/收起按钮
        self._btn_toggle = QPushButton("展开详情 ▾")
        self._btn_toggle.setFlat(True)
        self._btn_toggle.setCursor(Qt.CursorShape.PointingHandCursor)
        self._btn_toggle.setVisible(False)
        self._btn_toggle.setStyleSheet("""
            QPushButton {
                color: #4FC3F7; font-size: 11px; text-align: left; padding: 2px 26px; border: none;
            }
            QPushButton:hover { color: #81D4FA; }
        """)
        self._btn_toggle.clicked.connect(self._toggle_details)
        layout.addWidget(self._btn_toggle)

    def _toggle_details(self):
        self._detail_expanded = not self._detail_expanded
        self._detail_container.setVisible(self._detail_expanded)
        self._btn_toggle.setText("收起详情 ▴" if self._detail_expanded else "展开详情 ▾")

    def update_status(self, status: int):
        """更新卡片状态"""
        self._status = status
        border = STATUS_BORDER.get(status, "#3D3D3D")
        color = STATUS_COLORS.get(status, "#CCCCCC")
        icon = STATUS_ICONS.get(status, "○")

        self._lbl_icon.setText(icon)
        self._lbl_icon.setStyleSheet(f"color: {color}; font-size: 14px; font-weight: bold;")
        self._lbl_name.setStyleSheet(f"font-weight: bold; font-size: 13px; color: #E0E0E0;")
        self.setStyleSheet(self._card_style(border))

    def set_result(self, text: str):
        """设置结果文本 — 第一行为摘要，后续为详情（支持 HTML 颜色）"""
        self._result_text = text
        lines = text.strip().split("\n")
        summary = lines[0].strip() if lines else ""
        details = [l.strip() for l in lines[1:] if l.strip()]

        # 摘要行
        self._lbl_summary.setText(f"⮑  {summary}")
        self._lbl_summary.setVisible(True)

        # 判断颜色
        is_fail = "失败" in text or "异常" in text or "未" in text
        color = "#E0556A" if is_fail else "#4ECB71"
        self._lbl_summary.setStyleSheet(
            f"color: {color}; font-size: 12px; padding-left: 26px; padding-top: 4px; font-weight: bold;"
        )

        # 详情（支持 HTML 颜色标记）
        if details:
            self._detail_lines = details
            detail_html = "<br>".join(details)
            self._lbl_details.setTextFormat(Qt.TextFormat.RichText)
            self._lbl_details.setText(detail_html)
            self._lbl_details.setStyleSheet(
                "color: #AAAAAA; font-size: 11px; line-height: 1.8;"
            )
            self._btn_toggle.setVisible(True)
            # 默认展开详情
            self._detail_expanded = True
            self._detail_container.setVisible(True)
            self._btn_toggle.setText("收起详情 ▴")
        else:
            self._btn_toggle.setVisible(False)
            self._detail_container.setVisible(False)

    @staticmethod
    def _card_style(border_color: str) -> str:
        return f"""
            StepResultCard {{
                background-color: #252525;
                border: 1px solid {border_color};
                border-left: 3px solid {border_color};
                border-radius: 4px;
            }}
        """


class WorkflowTab(QWidget):
    """工作流执行标签页"""

    log_signal = Signal(str, str)
    step_status_signal = Signal(int, int)

    def __init__(self, runner: StepRunner, parent=None, build_params_collector=None):
        super().__init__(parent)
        self._runner = runner
        self._build_params_collector = build_params_collector  # callable → dict
        self._step_items: list[QListWidgetItem] = []
        self._step_states: dict[int, int] = {}
        self._result_cards: dict[int, StepResultCard] = {}
        self._running = False
        self._thread: QThread | None = None
        self._setup_ui()
        self._connect_signals()

    def _setup_ui(self):
        # 根布局
        root_layout = QVBoxLayout(self)
        root_layout.setContentsMargins(8, 8, 8, 8)
        root_layout.setSpacing(0)

        # ---- QSplitter 水平分割 ----
        self._splitter = QSplitter(Qt.Orientation.Horizontal)
        self._splitter.setChildrenCollapsible(False)
        self._splitter.setHandleWidth(2)
        self._splitter.setStyleSheet("QSplitter::handle { background-color: #3D3D3D; }")

        # ---- 左侧面板 ----
        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)
        left_layout.setContentsMargins(0, 0, 4, 0)
        left_layout.setSpacing(8)

        # 步骤清单
        step_group = QGroupBox("PSO 打包工作流")
        step_layout = QVBoxLayout(step_group)
        self._step_list = QListWidget()
        font = QFont()
        font.setPointSize(11)
        self._step_list.setFont(font)
        self._step_list.setSpacing(2)
        self._step_list.setMinimumWidth(280)
        self._populate_steps()
        step_layout.addWidget(self._step_list)
        left_layout.addWidget(step_group)

        # Step 4 控制面板（默认隐藏）
        self._step3_panel = QGroupBox("Step 4: 自动监听 PSO 记录")
        self._step3_panel.setVisible(False)
        self._step3_panel.setStyleSheet("""
            QGroupBox {
                border: 1px solid #F0C040;
                border-radius: 6px;
                margin-top: 8px;
                padding-top: 12px;
                color: #F0C040;
                font-weight: bold;
            }
        """)
        step3_layout = QVBoxLayout(self._step3_panel)
        self._lbl_step3_hint = QLabel(
            "请手动运行打包程序 (.exe)，遍历所有场景。\n"
            "工具每 2 秒自动检查 .rec 文件变化，稳定后自动继续。"
        )
        self._lbl_step3_hint.setWordWrap(True)
        self._lbl_step3_hint.setStyleSheet("color: #CCCCCC; padding: 4px 0;")
        step3_layout.addWidget(self._lbl_step3_hint)

        self._lbl_step3_status = QLabel("等待 .rec 文件...")
        self._lbl_step3_status.setStyleSheet("color: #4FC3F7; font-size: 13px; padding: 4px 0;")
        step3_layout.addWidget(self._lbl_step3_status)

        step3_btn_layout = QHBoxLayout()
        self._btn_step3_launch = QPushButton("打开打包程序")
        self._btn_step3_launch.setMinimumHeight(32)
        self._btn_step3_launch.setStyleSheet("""
            QPushButton { background-color: #388E3C; color: white; border: none; padding: 8px 16px; border-radius: 4px; font-weight: bold; }
            QPushButton:hover { background-color: #4CAF50; }
            QPushButton:disabled { background-color: #555; color: #888; }
        """)
        self._btn_step3_launch.clicked.connect(self._on_step3_launch)
        self._btn_step3_launch.setEnabled(False)
        step3_btn_layout.addWidget(self._btn_step3_launch)

        self._btn_step3_close_game = QPushButton("关闭游戏")
        self._btn_step3_close_game.setMinimumHeight(32)
        self._btn_step3_close_game.setStyleSheet("""
            QPushButton { background-color: #C62828; color: white; border: none; padding: 8px 16px; border-radius: 4px; font-weight: bold; }
            QPushButton:hover { background-color: #E53935; }
            QPushButton:disabled { background-color: #555; color: #888; }
        """)
        self._btn_step3_close_game.clicked.connect(self._on_step3_close_game)
        self._btn_step3_close_game.setEnabled(False)
        step3_btn_layout.addWidget(self._btn_step3_close_game)

        self._btn_step3_skip = QPushButton("手动继续（跳过等待）")
        self._btn_step3_skip.setObjectName("btnPrimary")
        self._btn_step3_skip.setMinimumHeight(32)
        self._btn_step3_skip.clicked.connect(self._on_step3_skip)
        step3_btn_layout.addWidget(self._btn_step3_skip)
        step3_btn_layout.addStretch()
        step3_layout.addLayout(step3_btn_layout)
        left_layout.addWidget(self._step3_panel)

        # Step 10 控制面板（默认隐藏）
        self._step9_panel = QGroupBox("Step 10: 测试 PSO 覆盖范围")
        self._step9_panel.setVisible(False)
        self._step9_panel.setStyleSheet("""
            QGroupBox {
                border: 1px solid #00897B;
                border-radius: 6px;
                margin-top: 8px;
                padding-top: 12px;
                color: #26A69A;
                font-weight: bold;
            }
        """)
        step9_layout = QVBoxLayout(self._step9_panel)
        self._lbl_step9_hint = QLabel(
            "游戏已启动：-logpso（生成 PSO 日志）  -clearPSODriverCache（清空驱动缓存）\n"
            "请遍历所有场景。测试完成后正常退出游戏，工具将自动分析 PSO 日志。"
        )
        self._lbl_step9_hint.setWordWrap(True)
        self._lbl_step9_hint.setStyleSheet("color: #CCCCCC; padding: 4px 0;")
        step9_layout.addWidget(self._lbl_step9_hint)

        self._lbl_step9_status = QLabel("等待游戏启动...")
        self._lbl_step9_status.setStyleSheet("color: #4FC3F7; font-size: 13px; padding: 4px 0;")
        step9_layout.addWidget(self._lbl_step9_status)

        step9_btn_layout = QHBoxLayout()
        self._btn_step9_launch = QPushButton("打开打包程序")
        self._btn_step9_launch.setMinimumHeight(32)
        self._btn_step9_launch.setStyleSheet("""
            QPushButton { background-color: #00695C; color: white; border: none; padding: 8px 16px; border-radius: 4px; font-weight: bold; }
            QPushButton:hover { background-color: #00897B; }
            QPushButton:disabled { background-color: #555; color: #888; }
        """)
        self._btn_step9_launch.clicked.connect(self._on_step9_launch)
        self._btn_step9_launch.setEnabled(False)
        step9_btn_layout.addWidget(self._btn_step9_launch)

        self._btn_step9_close_game = QPushButton("关闭游戏")
        self._btn_step9_close_game.setMinimumHeight(32)
        self._btn_step9_close_game.setStyleSheet("""
            QPushButton { background-color: #C62828; color: white; border: none; padding: 8px 16px; border-radius: 4px; font-weight: bold; }
            QPushButton:hover { background-color: #E53935; }
            QPushButton:disabled { background-color: #555; color: #888; }
        """)
        self._btn_step9_close_game.clicked.connect(self._on_step9_close_game)
        self._btn_step9_close_game.setEnabled(False)
        step9_btn_layout.addWidget(self._btn_step9_close_game)
        step9_btn_layout.addStretch()
        step9_layout.addLayout(step9_btn_layout)
        left_layout.addWidget(self._step9_panel)

        # 进度条
        self._progress_bar = QProgressBar()
        self._progress_bar.setMinimum(0)
        self._progress_bar.setMaximum(11)
        self._progress_bar.setValue(0)
        self._progress_bar.setTextVisible(True)
        self._progress_bar.setFormat("进度: %v / 11")
        left_layout.addWidget(self._progress_bar)

        # 按钮组
        btn_layout = QHBoxLayout()
        self._btn_run_all = QPushButton("▶ 全部执行")
        self._btn_run_all.setObjectName("btnPrimary")
        self._btn_run_all.clicked.connect(self._on_run_all)
        btn_layout.addWidget(self._btn_run_all)

        self._btn_run_selected = QPushButton("执行选中步")
        self._btn_run_selected.clicked.connect(self._on_run_selected)
        btn_layout.addWidget(self._btn_run_selected)

        self._btn_run_all_ci = QPushButton("执行全部CI流程")
        self._btn_run_all_ci.setObjectName("btnAllCI")
        self._btn_run_all_ci.setMinimumHeight(32)
        self._btn_run_all_ci.setStyleSheet("""
            QPushButton#btnAllCI {
                background-color: #6A1B9A; color: white; border: none; padding: 8px 14px; border-radius: 4px; font-weight: bold;
            }
            QPushButton#btnAllCI:hover { background-color: #8E24AA; }
            QPushButton#btnAllCI:disabled { background-color: #555; color: #888; }
        """)
        self._btn_run_all_ci.clicked.connect(self._on_run_all_ci)
        btn_layout.addWidget(self._btn_run_all_ci)

        self._btn_run_step9 = QPushButton("测试 PSO 覆盖")
        self._btn_run_step9.setObjectName("btnStep9")
        self._btn_run_step9.setMinimumHeight(32)
        self._btn_run_step9.setStyleSheet("""
            QPushButton#btnStep9 {
                background-color: #00695C; color: white; border: none; padding: 8px 14px; border-radius: 4px; font-weight: bold;
            }
            QPushButton#btnStep9:hover { background-color: #00897B; }
            QPushButton#btnStep9:disabled { background-color: #555; color: #888; }
        """)
        self._btn_run_step9.clicked.connect(self._on_run_step9)
        btn_layout.addWidget(self._btn_run_step9)

        self._btn_stop = QPushButton("⏹ 停止")
        self._btn_stop.setObjectName("btnDanger")
        self._btn_stop.clicked.connect(self._on_stop)
        self._btn_stop.setEnabled(False)
        btn_layout.addWidget(self._btn_stop)
        btn_layout.addStretch()
        left_layout.addLayout(btn_layout)

        left_panel.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Expanding)

        # ---- 右侧面板：步骤详情 ----
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_layout.setContentsMargins(4, 0, 0, 0)
        right_layout.setSpacing(0)

        result_group = QGroupBox("步骤详情")
        result_group_layout = QVBoxLayout(result_group)
        result_group_layout.setContentsMargins(0, 4, 0, 0)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setStyleSheet("QScrollArea { background-color: #1E1E1E; border: none; }")

        self._cards_widget = QWidget()
        self._cards_layout = QVBoxLayout(self._cards_widget)
        self._cards_layout.setContentsMargins(4, 4, 4, 12)
        self._cards_layout.setSpacing(6)

        # 为每个步骤创建结果卡片
        for step in PSO_WORKFLOW_STEPS:
            card = StepResultCard(step)
            self._cards_layout.addWidget(card)
            self._result_cards[step.index] = card

        self._cards_layout.addStretch()
        scroll.setWidget(self._cards_widget)
        result_group_layout.addWidget(scroll)
        right_layout.addWidget(result_group)

        # ---- PSO 覆盖范围可视化面板（步骤详情下方） ----
        self._pso_coverage_panel = QGroupBox("PSO 覆盖范围")
        self._pso_coverage_panel.setFixedHeight(345)
        self._pso_coverage_panel.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self._pso_coverage_panel.setStyleSheet("""
            QGroupBox {
                border: 1px solid #3D3D3D;
                border-radius: 6px;
                margin-top: 8px;
                padding-top: 12px;
                color: #888888;
                font-weight: bold;
            }
        """)
        pso_cov_layout = QVBoxLayout(self._pso_coverage_panel)
        pso_cov_layout.setContentsMargins(14, 14, 14, 14)
        pso_cov_layout.setSpacing(8)

        # 空态提示
        self._pso_no_data_hint = QLabel("尚未执行 Step 10\n请点击「测试 PSO 覆盖」按钮运行后查看结果")
        self._pso_no_data_hint.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._pso_no_data_hint.setWordWrap(True)
        self._pso_no_data_hint.setStyleSheet("color: #666666; font-size: 12px; padding: 20px; font-weight: normal;")
        pso_cov_layout.addWidget(self._pso_no_data_hint)

        # 数据容器（默认隐藏）
        self._pso_data_container = QWidget()
        self._pso_data_container.setVisible(False)
        pso_data_layout = QVBoxLayout(self._pso_data_container)
        pso_data_layout.setContentsMargins(0, 0, 0, 0)
        pso_data_layout.setSpacing(8)

        # -- 覆盖率大进度条 --
        coverage_bar_layout = QVBoxLayout()
        coverage_label_row = QHBoxLayout()
        self._pso_coverage_label = QLabel("覆盖率")
        self._pso_coverage_label.setStyleSheet("color: #CCCCCC; font-size: 12px; font-weight: bold;")
        coverage_label_row.addWidget(self._pso_coverage_label)
        coverage_label_row.addStretch()
        self._pso_coverage_pct_label = QLabel("0%")
        self._pso_coverage_pct_label.setStyleSheet("color: #888888; font-size: 18px; font-weight: bold;")
        coverage_label_row.addWidget(self._pso_coverage_pct_label)
        coverage_bar_layout.addLayout(coverage_label_row)

        self._pso_coverage_bar = QProgressBar()
        self._pso_coverage_bar.setMinimum(0)
        self._pso_coverage_bar.setMaximum(100)
        self._pso_coverage_bar.setValue(0)
        self._pso_coverage_bar.setTextVisible(False)
        self._pso_coverage_bar.setFixedHeight(14)
        self._pso_coverage_bar.setStyleSheet("""
            QProgressBar {
                background-color: #333333;
                border: none;
                border-radius: 7px;
            }
            QProgressBar::chunk {
                background-color: #4ECB71;
                border-radius: 7px;
            }
        """)
        coverage_bar_layout.addWidget(self._pso_coverage_bar)
        pso_data_layout.addLayout(coverage_bar_layout)

        # -- 等级标签 --
        self._pso_grade_label = QLabel("")
        self._pso_grade_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._pso_grade_label.setStyleSheet("font-size: 14px; font-weight: bold; padding: 2px 0;")
        pso_data_layout.addWidget(self._pso_grade_label)

        # -- 三个指标卡片行（固定均分宽度） --
        self._pso_metrics_row = QHBoxLayout()
        self._pso_metrics_row.setSpacing(8)

        self._pso_card_cache = self._make_metric_card("📦 预编译缓存", "0")
        self._pso_card_new = self._make_metric_card("🆕 运行时新 PSO", "0")
        self._pso_card_total = self._make_metric_card("📊 游戏涉及 PSO", "0")

        self._pso_metrics_row.addWidget(self._pso_card_cache, 1)
        self._pso_metrics_row.addWidget(self._pso_card_new, 1)
        self._pso_metrics_row.addWidget(self._pso_card_total, 1)
        pso_data_layout.addLayout(self._pso_metrics_row)
        pso_data_layout.addSpacing(6)

        # -- 分段占比条 --
        self._pso_stack_bar_widget = QWidget()
        stack_bar_layout = QVBoxLayout(self._pso_stack_bar_widget)
        stack_bar_layout.setContentsMargins(0, 0, 0, 0)
        stack_bar_layout.setSpacing(3)

        # 分段条容器
        self._stack_segments_layout = QHBoxLayout()
        self._stack_segments_layout.setSpacing(2)
        self._stack_segments_layout.setContentsMargins(0, 0, 0, 0)
        self._stack_seg_cache = QLabel()
        self._stack_seg_cache.setFixedHeight(16)
        self._stack_seg_cache.setStyleSheet("background-color: #5B8DEF; border-radius: 3px; min-width: 4px;")
        self._stack_seg_new = QLabel()
        self._stack_seg_new.setFixedHeight(16)
        self._stack_seg_new.setStyleSheet("background-color: #F0A040; border-radius: 3px; min-width: 4px;")
        self._stack_seg_builtin = QLabel()
        self._stack_seg_builtin.setFixedHeight(16)
        self._stack_seg_builtin.setStyleSheet("background-color: #555555; border-radius: 3px; min-width: 4px;")
        self._stack_segments_layout.addWidget(self._stack_seg_cache)
        self._stack_segments_layout.addWidget(self._stack_seg_new)
        self._stack_segments_layout.addWidget(self._stack_seg_builtin)
        stack_bar_layout.addLayout(self._stack_segments_layout)

        # 图例行（三个图例均匀分布占满整行）
        legend_row = QHBoxLayout()
        legend_row.setSpacing(0)
        legend_row.setContentsMargins(4, 0, 4, 0)
        self._stack_legend_labels = []
        legend_row.addStretch()
        for color, label_text in [
            ("#5B8DEF", "预编译缓存"),
            ("#F0A040", "运行时新PSO"),
            ("#555555", "引擎内置"),
        ]:
            dot = QLabel()
            dot.setFixedSize(8, 8)
            dot.setStyleSheet(f"background-color: {color}; border-radius: 4px;")
            legend_row.addWidget(dot)
            legend_row.addSpacing(4)
            lbl = QLabel(label_text)
            lbl.setStyleSheet("color: #999999; font-size: 12px;")
            legend_row.addWidget(lbl)
            self._stack_legend_labels.append(lbl)
            legend_row.addStretch()
        stack_bar_layout.addLayout(legend_row)

        pso_data_layout.addWidget(self._pso_stack_bar_widget)

        # -- 缓存文件总条目（含引擎内置 PSO） --
        self._pso_total_saved_label = QLabel("")
        self._pso_total_saved_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._pso_total_saved_label.setStyleSheet("color: #999999; font-size: 12px; padding: 2px 0;")
        pso_data_layout.addWidget(self._pso_total_saved_label)

        # -- 日志信息 --
        self._pso_log_info = QLabel("")
        self._pso_log_info.setStyleSheet("color: #666666; font-size: 11px;")
        pso_data_layout.addWidget(self._pso_log_info)

        # -- 建议文本 --
        self._pso_advice_label = QLabel("")
        self._pso_advice_label.setWordWrap(True)
        self._pso_advice_label.setStyleSheet("color: #AAAAAA; font-size: 12px; padding-top: 2px;")
        pso_data_layout.addWidget(self._pso_advice_label)

        pso_cov_layout.addWidget(self._pso_data_container)
        right_layout.addWidget(self._pso_coverage_panel)

        right_panel.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        # 添加到 Splitter
        self._splitter.addWidget(left_panel)
        self._splitter.addWidget(right_panel)
        self._splitter.setStretchFactor(0, 3)   # 左侧 3
        self._splitter.setStretchFactor(1, 5)   # 右侧 5

        root_layout.addWidget(self._splitter)

    def _populate_steps(self):
        for step in PSO_WORKFLOW_STEPS:
            icon = STATUS_ICONS[StepStatus.PENDING.value]
            text = f"{icon}  Step {step.index}: {step.name}"
            item = QListWidgetItem(text)
            item.setData(Qt.ItemDataRole.UserRole, step.index)
            self._step_list.addItem(item)
            self._step_items.append(item)
            self._step_states[step.index] = StepStatus.PENDING.value

    def _connect_signals(self):
        self._runner.log_signal.connect(
            lambda level, msg: self.log_signal.emit(level, msg)
        )
        self._runner.step_status_signal.connect(self._on_step_status)
        self._runner.step_result_signal.connect(self._on_step_result)
        self._runner.need_user_input.connect(self._on_step3_hint)
        self._runner.step3_auto_mode_signal.connect(self._on_step3_auto_mode)
        self._runner.step3_exe_found_signal.connect(self._on_step3_exe_found)
        self._runner.step3_game_running_signal.connect(self._on_step3_game_running)
        self._runner.step9_panel_signal.connect(self._on_step9_panel)
        self._runner.step9_game_running_signal.connect(self._on_step9_game_running)
        self._runner.pso_coverage_data.connect(self._on_pso_coverage_data)
        self._runner.ask_close_editor.connect(self._on_ask_close_editor)
        self._runner.ask_skip_ci.connect(self._on_ask_skip_ci)
        self._runner.ask_ini_fix.connect(self._on_ask_ini_fix)
        self._runner.all_done_signal.connect(self._on_all_done)

    # ---- 执行控制 ----

    def _sync_build_params(self):
        """将参数管理页面的当前值同步到 runner，确保 UAT 参数正确"""
        if self._build_params_collector:
            params = self._build_params_collector()
            if params:
                self._runner.set_ui_params(params)

    def _on_run_all(self):
        if self._running:
            return
        self._sync_build_params()
        self._start_run(self._runner.run_all)

    def _on_run_selected(self):
        if self._running:
            return
        row = self._step_list.currentRow()
        if row < 0:
            return
        self._sync_build_params()
        self._start_run(lambda: self._runner.run_step(row))

    def _on_run_all_ci(self):
        """单独执行 Step 2: 执行全部CI流程"""
        if self._running:
            return
        self._start_run(lambda: self._runner.run_step(2))

    def _on_run_step9(self):
        """单独执行 Step 10: 测试 PSO 覆盖范围"""
        if self._running:
            return
        self._start_run(lambda: self._runner.run_step(10))

    def _start_run(self, target):
        if self._running:
            return
        self._set_running(True)
        self._thread = _WorkerThread(target)
        self._thread.finished.connect(self._on_thread_finished)
        self._thread.start()

    def _on_stop(self):
        self._runner.stop()
        self._btn_stop.setEnabled(False)

    def _on_thread_finished(self):
        self._set_running(False)
        self._step3_panel.setVisible(False)
        self._step9_panel.setVisible(False)

    def _on_all_done(self):
        self._set_running(False)
        self._step3_panel.setVisible(False)
        self._step9_panel.setVisible(False)

    def _set_running(self, running: bool):
        self._running = running
        self._btn_run_all.setEnabled(not running)
        self._btn_run_selected.setEnabled(not running)
        self._btn_run_all_ci.setEnabled(not running)
        self._btn_run_step9.setEnabled(not running)
        self._btn_stop.setEnabled(running)
        if not running and self._thread is not None:
            self._thread.wait(5000)
            self._thread = None

    # ---- 状态更新 ----

    def _on_step_status(self, step_index: int, status_value: int):
        self._step_states[step_index] = status_value
        self._update_step_item(step_index)
        self._update_progress()

        # 同步更新右侧卡片状态
        card = self._result_cards.get(step_index)
        if card:
            card.update_status(status_value)

        self.step_status_signal.emit(step_index, status_value)

    def _on_step_result(self, step_index: int, result_text: str):
        """接收步骤执行结果摘要"""
        card = self._result_cards.get(step_index)
        if card:
            card.set_result(result_text)

    def _update_step_item(self, step_index: int):
        if step_index >= len(self._step_items):
            return
        step = PSO_WORKFLOW_STEPS[step_index]
        icon = STATUS_ICONS.get(self._step_states[step_index], "○")
        text = f"{icon}  Step {step.index}: {step.name}"
        item = self._step_items[step_index]
        item.setText(text)
        status = self._step_states[step_index]
        if status == StepStatus.RUNNING.value:
            item.setForeground(Qt.GlobalColor.cyan)
        elif status == StepStatus.SUCCESS.value:
            item.setForeground(Qt.GlobalColor.green)
        elif status == StepStatus.FAILED.value:
            item.setForeground(Qt.GlobalColor.red)
        else:
            item.setForeground(Qt.GlobalColor.gray)

    def _update_progress(self):
        done = sum(
            1 for s in self._step_states.values()
            if s in (StepStatus.SUCCESS.value, StepStatus.FAILED.value)
        )
        self._progress_bar.setValue(done)

    # ---- Step 4 ----

    def _on_step3_hint(self, instructions: str):
        pass

    def _on_step3_auto_mode(self, active: bool):
        self._step3_panel.setVisible(active)
        if active:
            self._lbl_step3_status.setText("等待 .rec.upipelinecache 文件...")
            self._btn_step3_skip.setEnabled(True)
            self._btn_step3_launch.setEnabled(False)

    def _on_step3_exe_found(self, exe_path: str):
        if exe_path:
            self._lbl_step3_status.setText(f"已启动: {Path(exe_path).name}")
            self._btn_step3_launch.setEnabled(True)
            self._btn_step3_launch.setToolTip(exe_path)
        else:
            self._lbl_step3_status.setText("未找到打包程序，请手动打开")

    def _on_step3_launch(self):
        self._btn_step3_launch.setEnabled(False)
        self._lbl_step3_status.setText("正在打开打包程序...")
        self._runner.launch_packaged_game()

    def _on_step3_game_running(self, running: bool):
        self._btn_step3_close_game.setEnabled(running)

    def _on_step3_close_game(self):
        self._btn_step3_close_game.setEnabled(False)
        self._lbl_step3_status.setText("正在关闭游戏...")
        self._runner.close_packaged_game()
        # 直接 taskkill 确保进程真正终止（Popen.terminate() 可能漏掉子进程）
        self._force_kill_game_by_name()

    def _on_step3_skip(self):
        self._btn_step3_skip.setEnabled(False)
        self._lbl_step3_status.setText("用户手动跳过，正在继续...")
        self._runner.skip_step3_wait()

    # ---- Step 10 ---- 

    def _on_step9_panel(self, active: bool):
        """Step 10 面板显示/隐藏"""
        self._step9_panel.setVisible(active)
        if active:
            self._lbl_step9_status.setText("等待游戏启动...")
            self._btn_step9_launch.setEnabled(False)

    def _on_step9_game_running(self, running: bool):
        """Step 10 游戏进程运行状态"""
        self._btn_step9_close_game.setEnabled(running)
        if running:
            self._lbl_step9_status.setText("游戏运行中，请遍历场景后退出...")
            self._btn_step9_launch.setEnabled(False)
        else:
            self._lbl_step9_status.setText("游戏已退出，正在分析日志...")
            self._btn_step9_launch.setEnabled(True)

    def _on_step9_launch(self):
        """Step 10 手动打开打包程序"""
        self._btn_step9_launch.setEnabled(False)
        self._lbl_step9_status.setText("正在打开打包程序...")
        self._runner.launch_step9_game()

    def _on_step9_close_game(self):
        """Step 10 关闭游戏（设置标志 + 直接 taskkill 确保游戏真正终止）"""
        self._btn_step9_close_game.setEnabled(False)
        self._lbl_step9_status.setText("正在关闭游戏...")
        self._runner.close_step9_game()
        self._force_kill_game_by_name()

    def _force_kill_game_by_name(self):
        """通过 taskkill /F 直接终止游戏进程（参照 build_params_tab 的可靠做法）"""
        proj = getattr(self._runner, '_project', None)
        game_exe_path = getattr(self._runner, '_game_exe_path', '')
        exe_name = None
        if game_exe_path:
            exe_name = Path(game_exe_path).name
        elif proj and proj.name:
            exe_name = f"{proj.name}.exe"
        if not exe_name:
            return
        try:
            result = subprocess.run(
                ["taskkill", "/F", "/IM", exe_name],
                capture_output=True, text=True, timeout=10,
            )
            if result.returncode == 0:
                self._lbl_step3_status.setText(f"已关闭: {exe_name}")
                self._lbl_step9_status.setText(f"已关闭: {exe_name}")
        except Exception:
            pass

    # ---- PSO 覆盖范围可视化 ----

    @staticmethod
    def _make_metric_card(title: str, value: str) -> QFrame:
        """创建指标卡片"""
        card = QFrame()
        card.setFrameShape(QFrame.Shape.StyledPanel)
        card.setStyleSheet("""
            QFrame {
                background-color: #2A2A2A;
                border: 1px solid #3D3D3D;
                border-radius: 4px;
            }
        """)
        layout = QVBoxLayout(card)
        layout.setContentsMargins(8, 6, 8, 6)
        layout.setSpacing(2)

        lbl_title = QLabel(title)
        lbl_title.setStyleSheet("color: #888888; font-size: 12px; font-weight: bold; border: none; background: transparent;")
        lbl_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(lbl_title)

        lbl_value = QLabel(value)
        lbl_value.setStyleSheet("color: #E0E0E0; font-size: 15px; font-weight: bold; border: none; background: transparent;")
        lbl_value.setAlignment(Qt.AlignmentFlag.AlignCenter)
        lbl_value.setObjectName("metric_value")
        layout.addWidget(lbl_value)

        card._value_label = lbl_value
        return card

    def _on_pso_coverage_data(self, data: dict):
        """接收 Step 10 的 PSO 覆盖数据，更新可视化面板"""
        coverage_pct = data.get("coverage_pct", 0.0)
        cache_entries = data.get("cache_entries", 0)
        new_pso_count = data.get("new_pso_count", 0)
        game_used = data.get("game_used", cache_entries + new_pso_count)
        total_saved = data.get("total_saved", 0)
        grade = data.get("grade", "")
        advice = data.get("advice", "")
        log_file_name = data.get("log_file_name", "")
        log_file_size_kb = data.get("log_file_size_kb", 0.0)

        # 根据覆盖率确定颜色
        if coverage_pct >= 95:
            bar_color = "#4ECB71"
            pct_color = "#4ECB71"
            grade_color = "#4ECB71"
            border_color = "#4ECB71"
        elif coverage_pct >= 70:
            bar_color = "#F0C040"
            pct_color = "#F0C040"
            grade_color = "#F0C040"
            border_color = "#F0C040"
        elif coverage_pct > 0:
            bar_color = "#E0556A"
            pct_color = "#E0556A"
            grade_color = "#E0556A"
            border_color = "#E0556A"
        else:
            bar_color = "#888888"
            pct_color = "#888888"
            grade_color = "#888888"
            border_color = "#3D3D3D"

        # 显示数据容器，隐藏空态提示
        self._pso_no_data_hint.setVisible(False)
        self._pso_data_container.setVisible(True)

        # Panel 边框颜色
        self._pso_coverage_panel.setStyleSheet(f"""
            QGroupBox {{
                border: 1px solid {border_color};
                border-radius: 6px;
                margin-top: 8px;
                padding-top: 12px;
                color: {grade_color};
                font-weight: bold;
            }}
        """)

        # 覆盖率百分比
        self._pso_coverage_pct_label.setText(f"{coverage_pct:.1f}%")
        self._pso_coverage_pct_label.setStyleSheet(f"color: {pct_color}; font-size: 18px; font-weight: bold;")

        # 进度条
        self._pso_coverage_bar.setValue(int(coverage_pct))
        self._pso_coverage_bar.setStyleSheet(f"""
            QProgressBar {{
                background-color: #333333;
                border: none;
                border-radius: 7px;
            }}
            QProgressBar::chunk {{
                background-color: {bar_color};
                border-radius: 7px;
            }}
        """)

        # 等级
        self._pso_grade_label.setText(grade)
        self._pso_grade_label.setStyleSheet(f"font-size: 14px; font-weight: bold; padding: 2px 0; color: {grade_color};")

        # 三张指标卡片
        self._set_card_value(self._pso_card_cache, str(cache_entries))
        self._set_card_value(self._pso_card_new, str(new_pso_count))
        self._set_card_value(self._pso_card_total, str(game_used))

        # 分段占比条
        engine_builtin = total_saved - game_used
        if total_saved > 0:
            self._pso_stack_bar_widget.setVisible(True)
            # 更新分段宽度（stretch 按数值比例分配）
            self._stack_segments_layout.setStretch(0, cache_entries)
            self._stack_segments_layout.setStretch(1, new_pso_count)
            self._stack_segments_layout.setStretch(2, engine_builtin)
            # 更新图例：附加数值和百分比
            self._update_stack_legend(cache_entries, new_pso_count, engine_builtin, total_saved)
        else:
            self._pso_stack_bar_widget.setVisible(False)

        # 缓存文件总条目（含引擎内置 PSO）
        if total_saved > 0:
            self._pso_total_saved_label.setText(f"缓存文件总条目: {total_saved}  引擎内置（{total_saved} - {game_used} = {engine_builtin}）")
        else:
            self._pso_total_saved_label.setText("")

        # 日志信息
        self._pso_log_info.setText(f"日志文件: {log_file_name}  ({log_file_size_kb:.1f} KB)")

        # 建议
        self._pso_advice_label.setText(f"💡 {advice}")

    def _update_stack_legend(self, cache_entries: int, new_pso_count: int, engine_builtin: int, total_saved: int = 0):
        """更新分段占比条图例的数值（含百分比）"""
        if total_saved > 0:
            names = [
                f"预编译缓存  {cache_entries} ({cache_entries / total_saved * 100:.1f}%)",
                f"运行时新PSO  {new_pso_count} ({new_pso_count / total_saved * 100:.1f}%)",
                f"引擎内置  {engine_builtin} ({engine_builtin / total_saved * 100:.1f}%)",
            ]
        else:
            names = [
                f"预编译缓存  {cache_entries}",
                f"运行时新PSO  {new_pso_count}",
                f"引擎内置  {engine_builtin}",
            ]
        for i, lbl in enumerate(self._stack_legend_labels):
            if i < len(names):
                lbl.setText(names[i])

    @staticmethod
    def _set_card_value(card: QFrame, value: str):
        """更新指标卡片的值"""
        lbl = card.findChild(QLabel, "metric_value")
        if lbl:
            lbl.setText(value)

    # ---- 编辑器占用弹窗 ----

    def _on_ask_close_editor(self, message: str):
        """编辑器占用导致清理失败，询问用户"""
        reply = QMessageBox.question(
            self,
            "编辑器正在运行",
            message,
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        should_close = (reply == QMessageBox.StandardButton.Yes)
        self._runner.on_editor_close_response(should_close)

    def _on_ask_skip_ci(self):
        """全部执行时，Step 2 弹窗询问是否跳过 CI 流程（10 秒倒计时）"""
        dlg = _CISkipDialog(self)
        dlg.exec()
        self._runner.on_skip_ci_response(dlg.should_skip)

    def _on_ask_ini_fix(self, summary: str, items: list):
        """Step 1 配置检查未通过，弹窗询问是否自动写入缺失的 INI 配置"""
        dlg = _PSOIniFixDialog(summary, items, self)
        if dlg.exec() == QDialog.DialogCode.Accepted:
            self._runner.on_ini_fix_response(True)
        else:
            self._runner.on_ini_fix_response(False)


class _PSOIniFixDialog(QDialog):
    """PSO INI 配置修复确认弹窗"""

    TYPE_ICONS = {
        "missing": "❌ 缺失",
        "wrong": "⚠️ 值不匹配",
        "nosection": "📂 节不存在",
    }

    def __init__(self, summary: str, items: list[dict], parent=None):
        super().__init__(parent)
        self.setWindowTitle("PSO 配置检查未通过")
        self.resize(600, 460)
        self.setMinimumSize(480, 320)
        self.setWindowModality(Qt.WindowModality.ApplicationModal)
        self.setWindowFlags(
            Qt.WindowType.Dialog
            | Qt.WindowType.WindowCloseButtonHint
            | Qt.WindowType.WindowTitleHint
        )
        self.setStyleSheet("""
            QDialog {
                background-color: #1E1E1E;
                color: #CCCCCC;
            }
            QLabel {
                color: #CCCCCC;
            }
            QScrollArea {
                background-color: #252525;
                border: 1px solid #3D3D3D;
                border-radius: 4px;
            }
            QPushButton {
                background-color: #388E3C;
                color: white;
                border: none;
                padding: 8px 20px;
                border-radius: 4px;
                font-weight: bold;
                min-width: 90px;
            }
            QPushButton:hover {
                background-color: #4CAF50;
            }
            QPushButton#skipBtn {
                background-color: #555555;
                color: #AAAAAA;
            }
            QPushButton#skipBtn:hover {
                background-color: #666666;
                color: #CCCCCC;
            }
        """)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 12, 16, 12)
        layout.setSpacing(10)

        # 标题
        title = QLabel("PSO 配置检查未通过")
        title.setStyleSheet("font-size: 15px; font-weight: bold; color: #E0556A;")
        layout.addWidget(title)

        # 摘要
        lbl_summary = QLabel(summary)
        lbl_summary.setStyleSheet("color: #AAAAAA; font-size: 12px;")
        layout.addWidget(lbl_summary)

        # 分隔线
        sep = QFrame()
        sep.setFrameShape(QFrame.Shape.HLine)
        sep.setStyleSheet("border-color: #3D3D3D; max-height: 1px;")
        layout.addWidget(sep)

        # 配置项列表
        lbl_list_title = QLabel(f"以下 {len(items)} 项配置需要修复：")
        lbl_list_title.setStyleSheet("color: #CCCCCC; font-size: 12px; font-weight: bold;")
        layout.addWidget(lbl_list_title)

        # 可滚动配置列表
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)

        list_widget = QWidget()
        list_layout = QVBoxLayout(list_widget)
        list_layout.setContentsMargins(0, 0, 0, 0)
        list_layout.setSpacing(4)

        # 按文件分组显示
        by_file: dict[str, list[dict]] = {}
        for item in items:
            by_file.setdefault(item["filename"], []).append(item)

        for filename, file_items in by_file.items():
            # 文件名标签
            file_lbl = QLabel(f"📄 {filename}")
            file_lbl.setStyleSheet("color: #4FC3F7; font-size: 12px; font-weight: bold; padding: 2px 0;")
            list_layout.addWidget(file_lbl)

            for item in file_items:
                type_icon = self.TYPE_ICONS.get(item.get("type", "missing"), "❓")
                item_value = item.get("value", "")
                # 空值显示为「需手动填写」
                display_value = item_value if item_value else "(需手动填写)"

                if item.get("type") == "wrong":
                    current_val = item.get('current_value', '?')
                    suggest_val = display_value
                    detail = f"      {type_icon}  [{item['section']}] {item['key']}"
                    detail2 = f"         当前: {current_val}  →  建议: {suggest_val}"
                    row = QLabel(detail)
                    row.setStyleSheet("color: #F0C040; font-size: 11px;")
                    list_layout.addWidget(row)
                    row2 = QLabel(detail2)
                    row2.setStyleSheet("color: #888888; font-size: 10px; padding-left: 12px;")
                    list_layout.addWidget(row2)
                else:
                    detail = f"      {type_icon}  [{item['section']}] {item['key']} = {display_value}"
                    row = QLabel(detail)
                    row.setStyleSheet("color: #E0556A; font-size: 11px;")
                    list_layout.addWidget(row)

                desc_lbl = QLabel(f"          {item['description']}")
                desc_lbl.setStyleSheet("color: #666666; font-size: 10px; padding-left: 12px;")
                list_layout.addWidget(desc_lbl)

        list_layout.addStretch()
        scroll.setWidget(list_widget)
        layout.addWidget(scroll, stretch=1)

        # 按钮区
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()

        skip_btn = QPushButton("跳过")
        skip_btn.setObjectName("skipBtn")
        skip_btn.clicked.connect(self.reject)
        btn_layout.addWidget(skip_btn)

        fix_btn = QPushButton("写入配置")
        fix_btn.clicked.connect(self.accept)
        btn_layout.addWidget(fix_btn)

        layout.addLayout(btn_layout)

class _CISkipDialog(QDialog):
    """CI 流程跳过确认弹窗（10 秒倒计时，超时后不跳过）"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("CI 流程确认")
        self.setFixedSize(400, 160)
        self._countdown = 10
        self._should_skip = False  # 默认不跳过

        self.setWindowModality(Qt.WindowModality.ApplicationModal)
        self.setWindowFlags(
            Qt.WindowType.Dialog
            | Qt.WindowType.WindowCloseButtonHint
            | Qt.WindowType.WindowTitleHint
        )
        self.setStyleSheet("""
            QDialog {
                background-color: #1E1E1E;
                color: #CCCCCC;
            }
            QLabel {
                color: #E0E0E0;
                background: transparent;
            }
            QPushButton {
                background-color: #3D3D3D;
                color: #E0E0E0;
                border: 1px solid #4D4D4D;
                border-radius: 4px;
                padding: 8px 18px;
                font-weight: bold;
                min-width: 100px;
            }
            QPushButton:hover {
                background-color: #4D4D4D;
            }
            QPushButton#btnContinue {
                background-color: #0078D4;
                color: white;
                border: none;
            }
            QPushButton#btnContinue:hover {
                background-color: #1A8CE8;
            }
        """)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 16, 20, 16)
        layout.setSpacing(12)

        # 提示文字
        lbl_title = QLabel("CI 流程会很长，是否跳过？")
        lbl_title.setStyleSheet("font-size: 14px; font-weight: bold; color: #E0E0E0;")
        layout.addWidget(lbl_title)

        # 倒计时：明确告知超时后不跳过
        self._lbl_countdown = QLabel(f"{self._countdown} 秒后不跳过，自动执行")
        self._lbl_countdown.setStyleSheet("color: #F0C040; font-size: 12px;")
        layout.addWidget(self._lbl_countdown)

        # 按钮区
        btn_layout = QHBoxLayout()
        btn_layout.setSpacing(12)

        self._btn_skip = QPushButton("跳过")
        self._btn_skip.clicked.connect(self._on_skip)
        btn_layout.addWidget(self._btn_skip)

        self._btn_continue = QPushButton("不跳过（继续执行）")
        self._btn_continue.setObjectName("btnContinue")
        self._btn_continue.clicked.connect(self._on_continue)
        btn_layout.addWidget(self._btn_continue)

        layout.addLayout(btn_layout)

        # 倒计时定时器
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self._timer.start(1000)

    def _tick(self):
        self._countdown -= 1
        if self._countdown <= 0:
            self._timer.stop()
            self._should_skip = False  # 超时不跳过
            self.accept()
        else:
            self._lbl_countdown.setText(f"{self._countdown} 秒后不跳过，自动执行")

    def _on_skip(self):
        self._timer.stop()
        self._should_skip = True
        self.accept()

    def _on_continue(self):
        self._timer.stop()
        self._should_skip = False
        self.accept()

    @property
    def should_skip(self) -> bool:
        return self._should_skip
