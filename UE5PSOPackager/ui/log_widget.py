"""
LogWidget - 日志显示组件
QPlainTextEdit 只读、彩色日志、右键菜单
"""

from PySide6.QtWidgets import QPlainTextEdit, QMenu
from PySide6.QtGui import QTextCharFormat, QColor, QFont, QAction
from PySide6.QtCore import Qt, Signal


LOG_COLORS = {
    "INFO":    QColor("#CCCCCC"),  # 白色/灰色 普通信息
    "SUCCESS": QColor("#4ECB71"),  # 绿色 成功
    "WARNING": QColor("#F0C040"),  # 黄色 警告
    "ERROR":   QColor("#E0556A"),  # 红色 错误
}


class LogWidget(QPlainTextEdit):
    """日志显示组件"""

    export_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)

        self.setReadOnly(True)
        self.setMaximumBlockCount(5000)  # 最多保留 5000 行

        # 等宽字体
        font = QFont("Cascadia Code", 10)
        font.setStyleHint(QFont.StyleHint.Monospace)
        self.setFont(font)

        # 启用右键菜单
        self.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.customContextMenuRequested.connect(self._show_context_menu)

        self._formats = {
            level: self._make_format(color)
            for level, color in LOG_COLORS.items()
        }

    def _make_format(self, color: QColor) -> QTextCharFormat:
        fmt = QTextCharFormat()
        fmt.setForeground(color)
        return fmt

    def append_log(self, level: str, message: str):
        """追加彩色日志行"""
        fmt = self._formats.get(level, self._formats["INFO"])
        self.setCurrentCharFormat(fmt)
        self.appendPlainText(message)

    def clear_log(self):
        """清空日志"""
        self.clear()

    def _show_context_menu(self, pos):
        menu = QMenu(self)
        menu.setStyleSheet(self._menu_style())

        action_clear = QAction("清除日志", self)
        action_clear.triggered.connect(self.clear_log)
        menu.addAction(action_clear)

        action_export = QAction("导出日志到 .txt", self)
        action_export.triggered.connect(self.export_requested.emit)
        menu.addAction(action_export)

        menu.exec(self.mapToGlobal(pos))

    def _menu_style(self) -> str:
        return """
            QMenu {
                background-color: #2D2D2D;
                color: #CCCCCC;
                border: 1px solid #3D3D3D;
                padding: 4px;
            }
            QMenu::item {
                padding: 6px 24px;
            }
            QMenu::item:selected {
                background-color: #0078D4;
            }
        """
