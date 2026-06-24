"""
MainWindow - 主窗口
顶部项目选择器 + 右侧 QTabWidget（配置/工作流/日志）
"""

from PySide6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTabWidget, QLabel, QComboBox, QFileDialog, QMessageBox,
)
from PySide6.QtCore import Qt

from config_manager import ConfigManager
from step_runner import StepRunner
from ui.config_tab import ConfigTab
from ui.workflow_tab import WorkflowTab
from ui.log_widget import LogWidget
from version import __version__


class MainWindow(QMainWindow):
    """主窗口"""

    def __init__(self, config: ConfigManager):
        super().__init__()

        self._config = config
        self._runner = StepRunner(config)
        self._current_project_index: int = -1

        self._setup_window()
        self._setup_menu()
        self._setup_ui()
        self._setup_style()

    # ---- 窗口设置 ----

    def _setup_window(self):
        self.setWindowTitle(f"UE5 PSOPackager  v{__version__}")
        self.resize(1550, 940)
        self.setMinimumSize(1200, 760)

    def _setup_menu(self):
        menu_bar = self.menuBar()

        # 文件菜单
        file_menu = menu_bar.addMenu("文件(&F)")
        act_reload = file_menu.addAction("重新加载配置")
        act_reload.triggered.connect(self._on_reload_config)
        file_menu.addSeparator()
        act_exit = file_menu.addAction("退出(&X)")
        act_exit.triggered.connect(self.close)

        # 帮助菜单
        help_menu = menu_bar.addMenu("帮助(&H)")
        act_about = help_menu.addAction("关于(&A)")
        act_about.triggered.connect(self._show_about)

    def _setup_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        layout.setContentsMargins(8, 4, 8, 8)
        layout.setSpacing(6)

        # ---- 顶部：标题 + 项目选择器 ----
        top_bar = QHBoxLayout()
        top_bar.setSpacing(8)

        lbl_title = QLabel("UE5 PSOPackager")
        lbl_title.setStyleSheet("font-size: 16px; font-weight: bold; color: #FFFFFF;")
        top_bar.addWidget(lbl_title)

        lbl_ver = QLabel(f"v{__version__}")
        lbl_ver.setStyleSheet("color: #888888; font-size: 11px;")
        top_bar.addWidget(lbl_ver)

        top_bar.addStretch()

        top_bar.addWidget(QLabel("当前项目:"))
        self._project_selector = QComboBox()
        self._project_selector.setMinimumWidth(200)
        self._project_selector.currentIndexChanged.connect(self._on_project_changed)
        top_bar.addWidget(self._project_selector)

        layout.addLayout(top_bar)

        # ---- 标签页 ----
        self._tab_widget = QTabWidget()

        self._config_tab = ConfigTab(self._config)
        self._config_tab.config_changed.connect(self._refresh_project_selector)
        self._tab_widget.addTab(self._config_tab, "配置管理")

        self._workflow_tab = WorkflowTab(self._runner)
        self._workflow_tab.log_signal.connect(self._on_log)
        self._tab_widget.addTab(self._workflow_tab, "工作流执行")

        self._log_widget = LogWidget()
        self._log_widget.export_requested.connect(self._export_log)
        self._tab_widget.addTab(self._log_widget, "日志输出")

        layout.addWidget(self._tab_widget)

        # 初始化
        self._refresh_project_selector()

        # 启动时输出已加载配置摘要到日志
        self._log_loaded_config()

    # ---- 项目切换 ----

    def _refresh_project_selector(self):
        self._project_selector.blockSignals(True)
        self._project_selector.clear()

        for proj in self._config.projects:
            self._project_selector.addItem(proj.name)

        idx = self._current_project_index
        if 0 <= idx < self._project_selector.count():
            self._project_selector.setCurrentIndex(idx)
        elif self._project_selector.count() > 0:
            self._project_selector.setCurrentIndex(0)
            self._current_project_index = 0
            self._runner.set_project(0)

        self._project_selector.blockSignals(False)

    def _on_project_changed(self, index: int):
        if index < 0:
            return
        self._current_project_index = index
        self._runner.set_project(index)
        self._log_widget.append_log("INFO", f"[项目切换] 当前项目: {self._config.projects[index].name}")

    # ---- 日志 ----

    def _on_log(self, level: str, message: str):
        self._log_widget.append_log(level, message)

    def _export_log(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "导出日志", "PSOPackager_Log.txt", "Text Files (*.txt)"
        )
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self._log_widget.toPlainText())
            self._log_widget.append_log("SUCCESS", f"日志已导出: {path}")

    # ---- 配置重载 ----

    def _log_loaded_config(self):
        """启动时输出已加载配置摘要到日志"""
        ue_count = len(self._config.ue5_versions)
        proj_count = len(self._config.projects)
        self._log_widget.append_log("SUCCESS", f"配置加载完成: {ue_count} 个 UE 版本, {proj_count} 个项目")
        for ver in self._config.ue5_versions:
            ue = self._config.ue5_versions[ver]
            path_info = ue.install_dir or "(未配置路径)"
            self._log_widget.append_log("INFO", f"  UE {ver}: {path_info}")
        for proj in self._config.projects:
            self._log_widget.append_log("INFO", f"  项目 [{proj.name}]: UE {proj.ue5_version}  |  {proj.project_dir or '(未配置目录)'}")

    def _on_reload_config(self):
        self._config.load()
        self._config_tab.refresh()
        self._refresh_project_selector()
        self._log_widget.append_log("SUCCESS", "配置已重新加载")

    # ---- 关于 ----

    def _show_about(self):
        QMessageBox.about(
            self, "关于 UE5 PSOPackager",
            f"<h3>UE5 PSOPackager v{__version__}</h3>"
            "<p>UE5 PSO 缓存打包自动化工具</p>"
            "<p>将 10 步 PSO 缓存收集、转换、集成打包流程自动化，"
            "消除运行时着色器编译卡顿。</p>"
            "<p><b>技术栈:</b> Python 3.10+ / PySide6 / PyInstaller</p>"
        )

    # ---- 样式 ----

    def _setup_style(self):
        self.setStyleSheet("""
            QMainWindow {
                background-color: #1E1E1E;
            }
            QWidget {
                background-color: #1E1E1E;
                color: #CCCCCC;
                font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
                font-size: 13px;
            }
            QTabWidget::pane {
                border: 1px solid #3D3D3D;
                background-color: #1E1E1E;
                border-radius: 0px;
            }
            QTabBar::tab {
                background-color: #2D2D2D;
                color: #AAAAAA;
                padding: 8px 20px;
                border: 1px solid #3D3D3D;
                border-bottom: none;
                margin-right: 2px;
            }
            QTabBar::tab:selected {
                background-color: #1E1E1E;
                color: #FFFFFF;
                border-bottom: 2px solid #0078D4;
            }
            QTabBar::tab:hover:!selected {
                background-color: #383838;
                color: #CCCCCC;
            }
            QGroupBox {
                border: 1px solid #3D3D3D;
                border-radius: 6px;
                margin-top: 12px;
                padding-top: 16px;
                font-weight: bold;
                color: #CCCCCC;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 12px;
                padding: 0 6px;
                color: #AAAAAA;
            }
            QLineEdit, QComboBox {
                background-color: #2D2D2D;
                border: 1px solid #4D4D4D;
                border-radius: 4px;
                padding: 6px 8px;
                color: #E0E0E0;
                selection-background-color: #0078D4;
            }
            QLineEdit:focus, QComboBox:focus {
                border: 1px solid #0078D4;
            }
            QLineEdit:disabled, QComboBox:disabled {
                background-color: #252525;
                color: #666666;
            }
            QComboBox::drop-down {
                border: none;
                width: 24px;
            }
            QComboBox QAbstractItemView {
                background-color: #2D2D2D;
                border: 1px solid #4D4D4D;
                color: #E0E0E0;
                selection-background-color: #0078D4;
            }
            QPushButton {
                background-color: #3D3D3D;
                color: #E0E0E0;
                border: 1px solid #4D4D4D;
                border-radius: 4px;
                padding: 6px 16px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #4D4D4D;
                border-color: #5D5D5D;
            }
            QPushButton:pressed {
                background-color: #2D2D2D;
            }
            QPushButton#btnPrimary {
                background-color: #0078D4;
                color: white;
                border: none;
            }
            QPushButton#btnPrimary:hover {
                background-color: #1A8CE8;
            }
            QPushButton#btnPrimary:pressed {
                background-color: #005A9E;
            }
            QPushButton#btnDanger {
                background-color: #C42B1C;
                color: white;
                border: none;
            }
            QPushButton#btnDanger:hover {
                background-color: #E04F3E;
            }
            QPushButton:disabled {
                background-color: #2D2D2D;
                color: #666666;
                border-color: #333333;
            }
            QListWidget {
                background-color: #252525;
                border: 1px solid #3D3D3D;
                border-radius: 4px;
                color: #E0E0E0;
                outline: none;
            }
            QListWidget::item {
                padding: 6px 10px;
                border-bottom: 1px solid #2D2D2D;
            }
            QListWidget::item:selected {
                background-color: #0078D4;
                color: white;
            }
            QListWidget::item:hover:!selected {
                background-color: #333333;
            }
            QProgressBar {
                background-color: #2D2D2D;
                border: 1px solid #3D3D3D;
                border-radius: 4px;
                text-align: center;
                color: #CCCCCC;
                height: 22px;
            }
            QProgressBar::chunk {
                background-color: #0078D4;
                border-radius: 3px;
            }
            QMenuBar {
                background-color: #2D2D2D;
                color: #CCCCCC;
                border-bottom: 1px solid #3D3D3D;
            }
            QMenuBar::item:selected {
                background-color: #0078D4;
            }
            QMenu {
                background-color: #2D2D2D;
                color: #CCCCCC;
                border: 1px solid #3D3D3D;
            }
            QMenu::item:selected {
                background-color: #0078D4;
            }
            QPlainTextEdit {
                background-color: #1A1A1A;
                border: 1px solid #3D3D3D;
                border-radius: 4px;
                color: #CCCCCC;
            }
            QScrollBar:vertical {
                background: #2D2D2D;
                width: 10px;
                border-radius: 5px;
            }
            QScrollBar::handle:vertical {
                background: #5D5D5D;
                border-radius: 5px;
                min-height: 30px;
            }
            QScrollBar::handle:vertical:hover {
                background: #7D7D7D;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
            QSplitter::handle {
                background-color: #3D3D3D;
                width: 2px;
            }
        """)
