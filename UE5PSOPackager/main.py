"""
UE5PSOPackager - 主入口
创建 QApplication，加载 ConfigManager，启动 MainWindow
"""

import sys
import os
from pathlib import Path

# 确保能正确导入项目模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QIcon

from config_manager import ConfigManager
from ui.main_window import MainWindow
from version import __version__


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("UE5PSOPackager")
    app.setApplicationVersion(__version__)
    app.setOrganizationName("UE5PSOPackager")

    # 加载配置
    config = ConfigManager()
    config.load()

    # 启动时展示配置摘要
    ue_count = len(config.ue5_versions)
    proj_count = len(config.projects)
    print(f"[INFO] 配置加载完成: {ue_count} 个 UE 版本, {proj_count} 个项目")
    for ver in config.ue5_versions:
        ue = config.ue5_versions[ver]
        print(f"  UE {ver}: {ue.install_dir or '(未配置路径)'}")
    for proj in config.projects:
        print(f"  项目 [{proj.name}]: UE {proj.ue5_version} | {proj.project_dir or '(未配置目录)'}")

    # 启动主窗口
    window = MainWindow(config)
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
