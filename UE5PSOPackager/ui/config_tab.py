"""
ConfigTab - 配置管理标签页
UE 版本管理 + 项目配置管理，支持多 UE 版本和多项目切换
"""

import json

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QListWidget, QListWidgetItem, QFormLayout,
    QLineEdit, QPushButton, QMessageBox, QLabel,
    QComboBox, QSplitter, QFileDialog, QDialog,
    QDialogButtonBox, QTextEdit,
)
from PySide6.QtCore import Qt, Signal
from pathlib import Path

from config_manager import ConfigManager, UE5VersionConfig, ProjectConfig


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


class ConfigTab(QWidget):
    """配置管理标签页"""

    config_changed = Signal()  # 配置变更通知

    def __init__(self, config: ConfigManager, parent=None):
        super().__init__(parent)
        self._config = config
        self._current_ue5_version: str = ""
        self._current_project_index: int = -1
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

        # 水平分割：左侧 UE 版本 + 项目列表，右侧表单
        splitter = QSplitter(Qt.Orientation.Horizontal)

        # ---- 左侧面板 ----
        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)
        left_layout.setContentsMargins(0, 0, 0, 0)

        # UE 版本组
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

        # 项目列表组
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

        # ---- 右侧面板 ----
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_layout.setContentsMargins(8, 0, 0, 0)

        # UE 详情表单（只需配置 UnrealEditor-Cmd.exe，其他自动推导）
        ue_form_group = QGroupBox("UE 引擎详情")
        ue_form_layout = QFormLayout(ue_form_group)

        # UnrealEditor-Cmd.exe 路径（唯一需要用户配置的字段）
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

        # 自动推导：引擎安装目录（只读显示）
        self._ue_install_label = QLabel("—")
        self._ue_install_label.setStyleSheet("color: #888888; padding: 6px 8px;")
        ue_form_layout.addRow("→ 引擎目录:", self._ue_install_label)

        # 自动推导：RunUAT.bat 路径（只读显示）
        self._ue_uat_label = QLabel("—")
        self._ue_uat_label.setStyleSheet("color: #888888; padding: 6px 8px;")
        ue_form_layout.addRow("→ RunUAT.bat:", self._ue_uat_label)

        # 验证状态
        self._ue_status = QLabel("")
        ue_form_layout.addRow("", self._ue_status)
        right_layout.addWidget(ue_form_group)

        # 项目详情表单
        proj_form_group = QGroupBox("项目详情")
        proj_form_layout = QFormLayout(proj_form_group)

        self._proj_name = QLineEdit()
        proj_form_layout.addRow("项目名称:", self._proj_name)

        self._proj_ue_version = QComboBox()
        self._proj_ue_version.setEditable(False)
        proj_form_layout.addRow("关联 UE 版本:", self._proj_ue_version)

        # 引擎版本匹配状态（从 .uproject 自动检测）
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
        self._proj_shk_rel.setText("Saved\\Cooked\\Windows\\{project_name}\\Metadata\\PipelineCaches")
        proj_form_layout.addRow("SHK 相对路径:", self._proj_shk_rel)

        self._proj_rec_rel = QLineEdit()
        self._proj_rec_rel.setText("Saved\\CollectedPSOs")
        proj_form_layout.addRow("REC 相对路径:", self._proj_rec_rel)

        self._proj_spc_rel = QLineEdit()
        self._proj_spc_rel.setText("Build\\Windows\\PipelineCaches")
        proj_form_layout.addRow("SPC 相对路径:", self._proj_spc_rel)

        # 验证状态
        self._proj_status = QLabel("")
        proj_form_layout.addRow("", self._proj_status)
        right_layout.addWidget(proj_form_group)

        splitter.addWidget(right_panel)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 2)

        layout.addWidget(splitter)

        # 所有控件初始化完成后，展示已加载的配置
        self.refresh()

    def refresh(self):
        """从 ConfigManager 刷新全部 UI 数据"""
        self._refresh_ue_list()
        self._refresh_ue_versions_combo()
        self._refresh_proj_list()

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

    def _render_ue_detail(self, version: str):
        ue = self._config.ue5_versions.get(version)
        if not ue:
            return

        self._ue_editor_cmd_path.blockSignals(True)
        self._ue_editor_cmd_path.setText(ue.editor_cmd_path)
        self._ue_editor_cmd_path.blockSignals(False)

        # 显示自动推导的路径
        self._show_derived_paths(ue)
        self._update_ue_status()

    def _show_derived_paths(self, ue: UE5VersionConfig):
        """显示从 editor_cmd_path 推导出的其他路径"""
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
        """当 UnrealEditor-Cmd.exe 路径变化时，自动推导其他路径"""
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

        # 设置关联 UE 版本
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

        # 显示引擎版本匹配状态（只读，不自动覆盖）
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
        """当 .uproject 路径变化时，自动读取 EngineAssociation 并匹配 UE 版本"""
        path = text.strip()
        if not path:
            self._proj_ue_status.clear()
            return

        uproject_path = Path(path)
        if not uproject_path.exists() or uproject_path.suffix.lower() != ".uproject":
            self._proj_ue_status.clear()
            return

        # 自动推导项目目录（.uproject 所在目录）
        proj_dir = str(uproject_path.parent)
        if self._proj_dir.text().strip() == "":
            self._proj_dir.blockSignals(True)
            self._proj_dir.setText(proj_dir)
            self._proj_dir.blockSignals(False)

        # 读取并自动匹配
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

        # 尝试匹配到已配置的 UE 版本（自动选择）
        idx = self._proj_ue_version.findText(engine_assoc)
        if idx >= 0:
            self._proj_ue_version.setCurrentIndex(idx)
            self._proj_ue_status.setText(f"✓ 已自动匹配 UE {engine_assoc}")
            self._proj_ue_status.setStyleSheet("color: #4ECB71; padding: 2px 8px;")
        else:
            self._proj_ue_status.setText(f"⚠ 引擎版本 '{engine_assoc}' 未在左侧配置，请先添加")
            self._proj_ue_status.setStyleSheet("color: #E0A800; padding: 2px 8px;")

    def _show_ue_match_status(self):
        """只读显示引擎版本匹配状态（不自动设置 combo，用于渲染已有项目）"""
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
        # 保存 UE 表单
        ver = self._current_ue5_version
        if ver and ver in self._config.ue5_versions:
            ue = self._config.ue5_versions[ver]
            ue.editor_cmd_path = self._ue_editor_cmd_path.text().strip()
            ue.resolve_from_editor_cmd()  # 自动推导 install_dir 和 uat_bat_path

        # 保存项目表单
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
