"""
PyInstaller 打包脚本
构建单文件 exe，排除无用 Qt 模块以减小体积
"""

import os
import subprocess
import sys
from pathlib import Path


def build():
    """执行 PyInstaller 打包命令"""
    root = Path(__file__).parent

    cmd_parts = [
        sys.executable, "-m", "PyInstaller",
        "--onefile",
        "--windowed",
        "--name", "UE5PSOPackager",
        "--icon", "NONE",
        "--add-data", f"resources{os.sep}default_config.json;resources{os.sep}",
    ]

    exclude_modules = [
        "PySide6.QtQml", "PySide6.QtQuick", "PySide6.QtWebEngine",
        "PySide6.QtWebChannel", "PySide6.QtPdf", "PySide6.QtNetwork",
        "PySide6.QtSql", "PySide6.QtTest", "PySide6.QtPrintSupport",
        "PySide6.QtSvg", "PySide6.QtMultimedia", "PySide6.QtMultimediaWidgets",
        "PySide6.QtSensors", "PySide6.QtSerialPort",
    ]

    for mod in exclude_modules:
        cmd_parts.extend(["--exclude-module", mod])

    cmd_parts.extend(["--clean", "--noconfirm", str(root / "main.py")])

    print("[BUILD] 开始打包...")
    print("[BUILD] " + " ".join(cmd_parts))

    result = subprocess.run(cmd_parts, cwd=str(root))
    if result.returncode == 0:
        exe_path = root / "dist" / "UE5PSOPackager.exe"
        size_mb = exe_path.stat().st_size / (1024 * 1024) if exe_path.exists() else 0
        print(f"[BUILD] 打包完成! 产物: {exe_path} ({size_mb:.1f} MB)")
    else:
        print(f"[BUILD] 打包失败! 返回码: {result.returncode}")
        sys.exit(result.returncode)


if __name__ == "__main__":
    build()
