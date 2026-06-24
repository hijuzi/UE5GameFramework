"""
StepRunner - PSO 打包步骤执行器
封装 Step 0-8 全部逻辑，支持 QThread 异步执行
通过信号槽与 UI 通信
"""

import os
import re
import shutil
import subprocess
import threading
import time
from datetime import datetime
from pathlib import Path
from typing import Optional, Callable

from PySide6.QtCore import QObject, Signal

from config_manager import ConfigManager, ProjectConfig, UE5VersionConfig
from step_definitions import StepStatus


# PSO 必需配置项定义（匹配 UE5 实际 INI 节名）
PSO_REQUIRED_CONFIGS = {
    "DefaultEngine.ini": {
        "ConsoleVariables": [
            ("r.ShaderPipelineCache.Enabled", "1", "启用 Shader Pipeline Cache"),
            ("r.ShaderPipelineCache.LogPSO", "1", "记录 PSO"),
            ("r.ShaderPipelineCache.SaveBoundPSOLog", "1", "保存绑定 PSO 日志"),
            ("r.ShaderPipelineCache.BackgroundBatchSize", None, "后台批处理大小"),
        ],
        "DevOptions.Shaders": [
            ("NeedsShaderStableKeys", "true", "启用 Shader 稳定键（Cook 时生成 .shk）"),
        ],
    },
    "DefaultGame.ini": {
        "/Script/UnrealEd.ProjectPackagingSettings": [
            ("bShareMaterialShaderCode", "True", "共享材质 Shader 代码"),
            ("bSharedMaterialNativeLibraries", "True", "共享材质 Native Shader 库（生成 .shk）"),
        ],
        "ShaderPipelineCache.CacheFile": [
            ("GameVersion", None, "PSO 缓存版本号"),
        ],
    },
}


class StepRunner(QObject):
    """PSO 打包步骤执行器"""

    # 信号定义
    log_signal = Signal(str, str)               # (level, message) 日志输出
    step_status_signal = Signal(int, int)       # (step_index, status) 步骤状态变更
    step_result_signal = Signal(int, str)       # (step_index, result_text) 步骤结果摘要
    step_progress_signal = Signal(int, int)     # (step_index, percentage) 步骤进度
    need_user_input = Signal(str)               # (message) 需要用户交互（Step 3 提示信息）
    step3_auto_mode_signal = Signal(bool)       # (active) Step 3 进入/退出自动监听模式
    step3_exe_found_signal = Signal(str)        # (exe_path) Step 3 找到的打包程序路径
    step3_game_running_signal = Signal(bool)    # (running) 游戏进程运行状态变化
    step9_panel_signal = Signal(bool)           # (active) Step 9 进入/退出面板
    step9_game_running_signal = Signal(bool)    # (running) Step 9 游戏进程运行状态
    pso_coverage_data = Signal(dict)            # (data) PSO 覆盖范围结果数据
    ask_close_editor = Signal(str)               # (message) 请求关闭编辑器
    all_done_signal = Signal()                  # 全部步骤完成

    LOG_LEVELS = ("INFO", "SUCCESS", "WARNING", "ERROR")

    def __init__(self, config: ConfigManager):
        super().__init__()
        self._config = config
        self._project: Optional[ProjectConfig] = None
        self._ue5: Optional[UE5VersionConfig] = None
        self._project_index: int = -1
        self._stop_requested = False
        self._step3_skip_wait = False
        self._step9_request_close = False       # Step 9 用户请求关闭游戏
        self._current_process: Optional[subprocess.Popen] = None
        self._game_process: Optional[subprocess.Popen] = None   # Step 3 启动的游戏进程
        self._game_exe_path: Optional[str] = None               # 游戏 exe 路径
        self._step_details: list[str] = []                      # 当前步骤执行详情项
        self._editor_close_response: Optional[bool] = None      # 编辑器关闭对话框响应
        self._editor_close_event: Optional[threading.Event] = None

    # ---- 公共接口 ----

    def set_project(self, index: int):
        """设置当前激活的项目"""
        self._project_index = index
        self._project = self._config.get_project(index)
        if self._project:
            self._ue5 = self._config.get_project_ue5(self._project)
        else:
            self._ue5 = None

    def stop(self):
        """请求停止执行"""
        self._stop_requested = True
        if self._current_process and self._current_process.poll() is None:
            self._current_process.terminate()
            self._log("WARNING", "已请求终止当前步骤")
        # 同时关闭可能正在运行的游戏进程
        self._terminate_game_process()

    def skip_step3_wait(self):
        """Step 3 中允许用户跳过自动监听，立即继续"""
        self._step3_skip_wait = True

    def launch_packaged_game(self):
        """由 UI 触发：查找并打开打包程序"""
        if not self._project:
            return
        exe_path = self._find_packaged_exe()
        if exe_path:
            self._open_exe(exe_path)
        else:
            self._log("WARNING", "未找到打包程序，请手动打开")

    def close_packaged_game(self):
        """由 UI 触发：关闭之前启动的打包程序
        不直接杀进程（避免阻塞 UI），改为设置跳过标志，
        让 Step3 循环在 worker 线程中自行终止进程并退出。"""
        self.skip_step3_wait()

    def launch_step9_game(self):
        """由 UI 触发：Step 9 用 -logpso 参数打开打包程序"""
        if not self._project:
            return
        exe_path = self._find_packaged_exe()
        if exe_path:
            exe_dir = Path(exe_path).parent
            log_dir = Path(self._project.output_dir) / "Windows" / self._project.name / "Saved" / "Logs"
            log_dir.mkdir(parents=True, exist_ok=True)
            log_file = log_dir / f"{self._project.name}_PSOTest.log"
            self._log("INFO", f"Step 9 启动游戏（-logpso，日志: {log_file}）")
            # 先关闭旧进程
            self._terminate_game_process()
            try:
                self._game_process = subprocess.Popen(
                    f'"{exe_path}" -logpso -log="{log_file}"',
                    cwd=str(exe_dir),
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                self._game_exe_path = exe_path
                self._log("SUCCESS", f"已启动游戏 (PID: {self._game_process.pid})")
                self.step9_game_running_signal.emit(True)
            except Exception as e:
                self._log("ERROR", f"无法启动游戏: {e}")

    def close_step9_game(self):
        """由 UI 触发：请求关闭 Step 9 游戏进程（避免阻塞 UI）"""
        self._step9_request_close = True

    def _find_packaged_exe(self) -> Optional[str]:
        """在打包输出目录中搜索 .exe 文件，返回完整路径或 None"""
        if not self._project:
            return None

        proj_name = self._project.name
        output_dir = Path(self._project.output_dir)

        if not output_dir.exists():
            self._log("WARNING", f"打包输出目录不存在: {output_dir}")
            return None

        # 按优先顺序尝试多个模式（UAT -archive 产出的常见路径结构）
        candidates = [
            output_dir / "Windows" / proj_name / f"{proj_name}.exe",
            output_dir / "Windows" / proj_name / "Binaries" / "Win64" / f"{proj_name}.exe",
            output_dir / "WindowsClient" / proj_name / f"{proj_name}.exe",
            output_dir / "WindowsClient" / proj_name / "Binaries" / "Win64" / f"{proj_name}.exe",
            output_dir / f"{proj_name}.exe",
        ]

        for candidate in candidates:
            if candidate.exists():
                self._log("SUCCESS", f"找到打包程序: {candidate}")
                return str(candidate)

        # 候选都不存在，尝试递归搜索（最多 2 层）
        self._log("INFO", "标准路径未找到，正在搜索打包程序...")
        try:
            for depth in (1, 2):
                for f in output_dir.glob("/".join(["*"] * depth) + f"/{proj_name}.exe"):
                    self._log("SUCCESS", f"找到打包程序: {f}")
                    return str(f)
        except Exception:
            pass

        return None

    def _open_exe(self, exe_path: str):
        """启动打包程序（非阻塞），并保存进程句柄以便后续关闭"""
        # 先关闭之前可能残留的旧进程
        self._terminate_game_process()

        try:
            exe_dir = Path(exe_path).parent
            self._game_process = subprocess.Popen(
                str(exe_path),
                cwd=str(exe_dir),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            self._game_exe_path = exe_path
            self._log("SUCCESS", f"已启动打包程序: {Path(exe_path).name}  (PID: {self._game_process.pid})")
            self.step3_game_running_signal.emit(True)
        except Exception as e:
            self._log("ERROR", f"无法启动打包程序: {e}")

    def _terminate_game_process(self):
        """终止 Step 3 启动的游戏进程"""
        if self._game_process is not None:
            proc = self._game_process
            self._game_process = None
            if proc.poll() is None:
                name = Path(self._game_exe_path).name if self._game_exe_path else "游戏"
                self._log("INFO", f"正在关闭 {name} (PID: {proc.pid})...")
                try:
                    proc.terminate()
                    try:
                        proc.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        self._log("WARNING", "进程未响应 terminate，尝试强制关闭...")
                        proc.kill()
                        proc.wait(timeout=5)
                    self._log("SUCCESS", f"{name} 已关闭")
                except Exception as e:
                    self._log("WARNING", f"关闭进程时出错: {e}")
            self.step3_game_running_signal.emit(False)

    def _terminate_game_by_name(self):
        """通过 exe 名称查找并终止残留的游戏进程"""
        if not self._project:
            return
        exe_name = f"{self._project.name}.exe"
        try:
            result = subprocess.run(
                ["taskkill", "/F", "/IM", exe_name],
                capture_output=True, text=True, timeout=10,
            )
            if result.returncode == 0:
                self._log("SUCCESS", f"已强制关闭所有 {exe_name} 进程")
            else:
                # 没有找到进程不算错误
                pass
        except Exception:
            pass

    def _ensure_game_closed(self):
        """在 UAT 打包前确保游戏进程已关闭，防止文件占用"""
        self._terminate_game_process()
        self._terminate_game_by_name()

    # ---- 编辑器进程 / 安全清理 ----

    def on_editor_close_response(self, should_close: bool):
        """UI 回调：用户选择是否关闭编辑器"""
        self._editor_close_response = should_close
        if self._editor_close_event:
            self._editor_close_event.set()

    def _is_editor_running(self) -> Optional[str]:
        """检测 UE Editor 是否在运行，返回进程名或 None"""
        try:
            result = subprocess.run(
                ["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe"],
                capture_output=True, text=True, timeout=5,
            )
            if "UnrealEditor.exe" in result.stdout:
                return "UnrealEditor.exe"
            result = subprocess.run(
                ["tasklist", "/FI", "IMAGENAME eq UnrealEditor-Cmd.exe"],
                capture_output=True, text=True, timeout=5,
            )
            if "UnrealEditor-Cmd.exe" in result.stdout:
                return "UnrealEditor-Cmd.exe"
        except Exception:
            pass
        return None

    def _kill_editor(self, editor_name: str) -> bool:
        """强制关闭编辑器进程"""
        try:
            result = subprocess.run(
                ["taskkill", "/F", "/IM", editor_name],
                capture_output=True, text=True, timeout=15,
            )
            return result.returncode == 0
        except Exception:
            return False

    def _safe_clean_dir(self, dir_path: Path, label: str) -> bool:
        """
        安全清理目录，返回是否成功。
        若失败且检测到 UE Editor 在运行，弹窗询问用户是否关闭编辑器。
        """
        if not dir_path.exists():
            return True

        def try_delete():
            try:
                shutil.rmtree(str(dir_path))
                return True
            except (PermissionError, OSError) as e:
                error_code = getattr(e, "winerror", 0) or 0
                self._log("WARNING", f"{label} 快速删除失败 (WinError {error_code}): {e}")
                # 逐文件清理
                try:
                    for root, dirs, files in os.walk(str(dir_path), topdown=False):
                        for name in files:
                            try:
                                (Path(root) / name).unlink()
                            except OSError:
                                pass
                        for name in dirs:
                            try:
                                (Path(root) / name).rmdir()
                            except OSError:
                                pass
                except OSError:
                    pass
                # 重新检查目录是否为空/不存在
                return not dir_path.exists() or not any(dir_path.iterdir())
            return False

        if try_delete():
            return True

        # 删除失败，检测是否 Editor 占用
        editor = self._is_editor_running()
        if editor:
            self._log("WARNING", f"检测到 {editor} 正在运行，可能阻止了文件清理")
            # 重置 event 等待 UI 响应
            self._editor_close_event = threading.Event()
            self._editor_close_response = None
            self.ask_close_editor.emit(
                f"{editor} 正在运行，导致「{label}」文件清理失败。\n是否关闭编辑器后重试？"
            )
            # 等待 UI 线程响应（最多 30 秒）
            responded = self._editor_close_event.wait(30)

            if responded and self._editor_close_response:
                # 用户选择关闭编辑器
                self._log("INFO", f"正在关闭 {editor}...")
                if self._kill_editor(editor):
                    self._log("SUCCESS", f"{editor} 已关闭")
                    # 重新尝试删除
                    if try_delete():
                        return True
                    self._log("WARNING", f"关闭编辑器后 {label} 仍无法完全清理")
                else:
                    self._log("WARNING", f"无法关闭 {editor}")
            else:
                self._log("WARNING", f"用户选择不关闭编辑器，跳过 {label} 清理")
        else:
            self._log("WARNING", f"{label} 清理失败，未检测到 UE Editor，可能有其他进程占用")

        return False  # 清理不完整，但不阻止后续步骤

    # ---- 步骤入口 ----

    def run_all(self):
        """顺序执行所有步骤（Step 0 ~ Step 9）"""
        self._stop_requested = False
        for step_index in range(10):
            if self._stop_requested:
                self._log("WARNING", "用户中止执行")
                break
            ok = self.run_step(step_index)
            if not ok:
                self._log("ERROR", f"步骤 {step_index} 失败，停止后续步骤")
                break
        self.all_done_signal.emit()

    def run_step(self, step_index: int) -> bool:
        """执行单个步骤，返回是否成功"""
        if self._project is None:
            self._log("ERROR", "未选择项目，请先在配置页面选择或创建项目")
            return False

        if self._project_index < 0:
            self._log("ERROR", "未选择项目")
            return False

        # 依赖 UE 引擎的步骤（Step 2/6/8）需要提前校验 UE 配置
        _UE_DEPENDENT_STEPS = {2, 6, 8}
        if step_index in _UE_DEPENDENT_STEPS and self._ue5 is None:
            self._log("ERROR",
                f"项目「{self._project.name}」关联的 UE 版本「{self._project.ue5_version}」"
                "未在配置中设置，请切换到「配置管理」页面检查 UE 引擎配置")
            self.step_status_signal.emit(step_index, StepStatus.FAILED.value)
            return False

        step_names = [
            "Step0_validate_config", "Step1_check_ini", "Step2_first_build",
            "Step3_collect_pso", "Step4_confirm_shk", "Step5_gather_files",
            "Step6_expand_spc", "Step7_integrate_spc", "Step8_final_build",
            "Step9_test_pso_coverage",
        ]

        if 0 <= step_index < len(step_names):
            method = getattr(self, step_names[step_index], None)
            if method:
                self.step_status_signal.emit(step_index, StepStatus.RUNNING.value)
                try:
                    self._step_details.clear()   # 每步开始时清空详情
                    start_time = time.time()
                    ok = method()
                    elapsed = time.time() - start_time
                    status = StepStatus.SUCCESS if ok else StepStatus.FAILED
                    self.step_status_signal.emit(step_index, status.value)
                    # 构建并发送步骤结果摘要（含详情列表）
                    summary, details = self._build_step_result(step_index, ok, elapsed)
                    result_text = summary + "\n" + details if details else summary
                    self.step_result_signal.emit(step_index, result_text)
                    return ok
                except Exception as e:
                    self._log("ERROR", f"步骤 {step_index} 异常: {e}")
                    self.step_status_signal.emit(step_index, StepStatus.FAILED.value)
                    self.step_result_signal.emit(step_index, f"异常: {e}")
                    return False
        return False

    # ---- 内部工具 ----

    def _log(self, level: str, message: str):
        """记录日志（通过信号发送到 UI）"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        full = f"[{timestamp}] {message}"
        self.log_signal.emit(level, full)

    def _run_cmd(self, cmd: str, cwd: str = None, timeout: int = None) -> tuple[bool, str]:
        """
        运行外部命令并逐行捕获输出（通过信号发射日志）
        返回 (成功, 完整输出)
        """
        self._log("INFO", f">> {cmd}")
        output_lines = []

        try:
            self._current_process = subprocess.Popen(
                cmd,
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                cwd=cwd,
                encoding="utf-8",
                errors="replace",
            )

            for line in self._current_process.stdout:
                if self._stop_requested:
                    self._current_process.terminate()
                    self._current_process.wait()
                    return False, "\n".join(output_lines)

                line = line.rstrip("\n\r")
                if line:
                    output_lines.append(line)
                    # 日志级别自动检测
                    lo = line.lower()
                    if "error" in lo or "fail" in lo:
                        self._log("ERROR", line)
                    elif "warn" in lo:
                        self._log("WARNING", line)
                    else:
                        self._log("INFO", line)

            self._current_process.wait()
            rc = self._current_process.returncode
            self._current_process = None

            ok = rc == 0
            if not ok:
                self._log("ERROR", f"命令返回码: {rc}")
            else:
                self._log("SUCCESS", f"命令执行成功 (返回码: 0)")

            return ok, "\n".join(output_lines)

        except Exception as e:
            self._current_process = None
            self._log("ERROR", f"命令执行异常: {e}")
            return False, str(e)

    def _check_file(self, path: str, description: str = "") -> bool:
        """检查文件/目录是否存在"""
        exists = Path(path).exists()
        label = description or path
        if exists:
            self._log("SUCCESS", f"  已找到: {label}")
        else:
            self._log("ERROR", f"  未找到: {label}")
        return exists

    def _resolve_project_name(self) -> str:
        """获取项目名称（用于路径替换）"""
        if self._project and self._project.name:
            return self._project.name
        return ""

    def _add_step_detail(self, line: str):
        """向当前步骤添加一条详情（显示在结果卡片中）"""
        self._step_details.append(line)

    def _build_step_result(self, step_index: int, ok: bool, elapsed: float) -> tuple[str, str]:
        """根据步骤索引和完成状态，构建 (结果摘要, 详情文本)"""
        proj = self._project
        elapsed_str = self._format_elapsed(elapsed)
        status_str = "成功" if ok else "失败"

        summaries = {
            0: f"{status_str} — 配置验证{'通过' if ok else '未通过'}  ({elapsed_str})",
            1: f"{status_str} — PSO INI 配置检查{'全部就绪' if ok else '存在问题'}  ({elapsed_str})",
            2: f"{status_str} — 首次打包{'完成' if ok else '失败'}  ({elapsed_str})",
            3: f"{status_str} — PSO 记录{'已收集' if ok else '收集中断'}  ({elapsed_str})",
            4: f"{status_str} — .shk 文件{'可用' if ok else '未找到'}  ({elapsed_str})",
            5: f"{status_str} — 缓存文件{'汇聚完成' if ok else '汇聚失败'}  ({elapsed_str})",
            6: f"{status_str} — .spc 转换{'完成' if ok else '失败'}  ({elapsed_str})",
            7: f"{status_str} — .spc 集成到 Build 目录  ({elapsed_str})",
            8: f"{status_str} — 最终打包{'完成' if ok else '失败'}  ({elapsed_str})",
            9: f"{status_str} — PSO 覆盖测试{'完成' if ok else '失败'}  ({elapsed_str})",
        }

        summary = summaries.get(step_index, f"步骤 {step_index} {status_str}  ({elapsed_str})")

        # 详情：每项前缀 ●
        detail_items = [f"  ●  {line}" for line in self._step_details]
        details = "\n".join(detail_items)

        return summary, details

    @staticmethod
    def _format_elapsed(seconds: float) -> str:
        """格式化耗时"""
        if seconds < 60:
            return f"{seconds:.1f}s"
        elif seconds < 3600:
            m, s = divmod(seconds, 60)
            return f"{int(m)}m {int(s)}s"
        else:
            h, r = divmod(seconds, 3600)
            m, s = divmod(r, 60)
            return f"{int(h)}h {int(m)}m {int(s)}s"

    # ---- Step 0: 验证配置 ----

    def Step0_validate_config(self) -> bool:
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 0: 验证配置")
        self._log("INFO", "=" * 50)

        errors = self._config.validate_all()
        if not errors:
            self._log("SUCCESS", "所有配置验证通过 ✓")
            self._log("INFO", f"  UE 版本数: {len(self._config.ue5_versions)}")
            self._log("INFO", f"  项目数: {len(self._config.projects)}")
        else:
            for err in errors:
                self._log("WARNING", f"  {err}")
            self._log("WARNING", f"发现 {len(errors)} 个配置问题，部分步骤可能无法执行")

        # -- 详情 --
        self._add_step_detail(f"UE 版本: {len(self._config.ue5_versions)} 个")
        for ver, uv in self._config.ue5_versions.items():
            self._add_step_detail(f"  └ UE {ver}: {uv.install_dir or '(未配置路径)'}  ({uv.editor_cmd_path or '未配置'})")
        self._add_step_detail(f"项目数: {len(self._config.projects)} 个")
        for p in self._config.projects:
            self._add_step_detail(f"  └ {p.name}  ({p.ue5_version})")
        if errors:
            self._add_step_detail(f"问题: {len(errors)} 项")
        else:
            self._add_step_detail("所有配置项验证通过")

        return len(errors) == 0

    # ---- Step 1: 检查 PSO INI 配置 ----

    def Step1_check_ini(self) -> bool:
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 1: 检查 PSO 项目配置 (.ini)")
        self._log("INFO", "=" * 50)

        if not self._project:
            return False

        proj_dir = Path(self._project.project_dir)
        config_dir = proj_dir / "Config"
        if not config_dir.exists():
            self._log("ERROR", f"项目 Config 目录不存在: {config_dir}")
            return False

        all_ok = True
        checked_count = 0
        ok_count = 0
        warn_count = 0
        missing_count = 0

        for ini_filename, sections in PSO_REQUIRED_CONFIGS.items():
            ini_path = config_dir / ini_filename
            self._log("INFO", f"检查 {ini_filename}...")
            self._add_step_detail(f"文件: {ini_filename}")

            if not ini_path.exists():
                self._log("WARNING", f"  文件不存在: {ini_path}")
                self._add_step_detail(f"  ⚠ 文件不存在")
                all_ok = False
                continue

            content = ini_path.read_text(encoding="utf-8", errors="replace")

            # 按 section 分组检查
            for section_name, configs in sections.items():
                section_pattern = re.compile(
                    rf'\[{re.escape(section_name)}\](.*?)(?=\[|$)',
                    re.DOTALL | re.IGNORECASE
                )
                match = section_pattern.search(content)

                if not match:
                    self._log("WARNING", f"  未找到 [{section_name}] 节")
                    self._add_step_detail(f"  [{section_name}] 未找到")
                    all_ok = False
                    continue

                section_content = match.group(1)

                for key, expected_value, description in configs:
                    checked_count += 1
                    key_pattern = re.compile(
                        rf'^[+\-.]?{re.escape(key)}\s*=\s*(.+)$',
                        re.MULTILINE | re.IGNORECASE
                    )
                    key_match = key_pattern.search(section_content)

                    if key_match:
                        value = key_match.group(1).strip()
                        if expected_value is None:
                            self._log("SUCCESS", f"  {key} = {value}  ({description}) ✓")
                            self._add_step_detail(f"  ✓ {key}={value}")
                            ok_count += 1
                        elif value == expected_value:
                            self._log("SUCCESS", f"  {key} = {value}  ({description}) ✓")
                            self._add_step_detail(f"  ✓ {key}={value}")
                            ok_count += 1
                        else:
                            self._log("WARNING", f"  {key} = {value}，建议值: {expected_value}  ({description})")
                            self._add_step_detail(f"  ⚠ {key}={value}（建议: {expected_value}）")
                            warn_count += 1
                    else:
                        self._log("ERROR", f"  缺少配置: {key}  ({description})")
                        self._add_step_detail(f"  ✗ {key} 缺失")
                        missing_count += 1
                        all_ok = False

        # 汇总
        self._add_step_detail(f"检查项: {checked_count} | 正确: {ok_count} | 警告: {warn_count} | 缺失: {missing_count}")

        if all_ok:
            self._log("SUCCESS", "INI PSO 配置检查全部通过 ✓")
        else:
            self._log("WARNING", "部分 INI 配置存在问题，请检查")

        return all_ok

    # ---- Step 2: 首次打包（生成 .shk） ----

    def Step2_first_build(self) -> bool:
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 2: 首次打包 — 生成 Shader 稳定键 (.shk)")
        self._log("INFO", "=" * 50)

        if self._project:
            self._add_step_detail(f"项目: {self._project.name}")
            self._add_step_detail(f"平台: {self._project.target_platform}")
            self._add_step_detail(f"Shader 格式: {', '.join(self._project.shader_formats)}")
            self._add_step_detail(f"输出: {self._project.output_dir}")

            # 首次打包前清理旧的打包输出和 Cook 缓存，确保干净构建

            # 0. 先备份旧包中已有的 PSO 记录（CollectedPSOs），防止清理后被误删
            backup_rec_dir = None
            old_rec_dir = (Path(self._project.output_dir) / "Windows"
                           / self._project.name / "Saved" / "CollectedPSOs")
            if old_rec_dir.exists():
                rec_files = list(old_rec_dir.glob("*.rec.upipelinecache"))
                if rec_files:
                    import tempfile
                    backup_rec_dir = Path(tempfile.mkdtemp(prefix="psopkg_backup_"))
                    backup_sub = backup_rec_dir / "CollectedPSOs"
                    backup_sub.mkdir(parents=True, exist_ok=True)
                    for rf in rec_files:
                        shutil.copy2(rf, backup_sub / rf.name)
                    self._log("INFO",
                              f"已备份旧包中 {len(rec_files)} 个 PSO 记录到临时目录")

            # 1. 清理打包输出目录（Package/）
            self._safe_clean_dir(
                Path(self._project.output_dir), "打包输出目录"
            )

            # 恢复备份的 PSO 记录到输出目录
            if backup_rec_dir and backup_rec_dir.exists():
                old_rec_dir.mkdir(parents=True, exist_ok=True)
                restored = 0
                for rf in (backup_rec_dir / "CollectedPSOs").glob("*.rec.upipelinecache"):
                    shutil.copy2(rf, old_rec_dir / rf.name)
                    restored += 1
                # 清理临时目录
                try:
                    shutil.rmtree(str(backup_rec_dir))
                except OSError:
                    pass
                self._log("INFO", f"已恢复 {restored} 个旧 PSO 记录到输出目录（作为兜底）")

            # 2. 清理 Cook 缓存，强制 UAT 做完全重新 Cook（避免增量 Cook 导致 .shk 与实际 Shader 不匹配）
            #    注意：只清理 Saved/Cooked，不需要清理 Saved/ShaderDebugInfo（那是 Shader 调试文件，不影响 Cook）
            self._safe_clean_dir(
                Path(self._project.project_dir) / "Saved/Cooked", "Saved/Cooked"
            )

        return self._run_uat_build("Step 2 首次打包")

    # ---- Step 3: 收集 PSO 记录（自动监听） ----

    def Step3_collect_pso(self) -> bool:
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 3: 收集 PSO 运行时记录（自动监听模式）")
        self._log("INFO", "=" * 50)

        proj = self._project
        if not proj:
            return False

        rec_dir = Path(proj.resolve_packaged_rec_source_dir())
        output_dir = proj.output_dir
        exe_dir = str(Path(output_dir) / "Windows" / proj.name / "Binaries" / "Win64")

        self._add_step_detail(f"监听目录: {rec_dir}")
        self._add_step_detail(f"打包输出: {output_dir}")
        self._add_step_detail(f"程序路径: {exe_dir}")

        self._log("INFO", "")
        self._log("INFO", "请手动运行打包后的程序 (.exe)，遍历所有场景：")
        self._log("INFO", f"  打包输出目录: {output_dir}")
        self._log("INFO", f"  PSO 记录目录: {rec_dir}")
        self._log("INFO", "")
        self._log("INFO", "工具将每 2 秒自动检查 .rec.upipelinecache 文件变化")
        self._log("INFO", "文件连续 5 秒无变化后自动继续下一步")
        self._log("INFO", "你也可以随时点击「手动继续」按钮跳过等待")
        self._log("INFO", "")

        # 通知 UI 进入自动监听模式
        self.step3_auto_mode_signal.emit(True)
        self._step3_skip_wait = False

        # 自动搜索并尝试打开打包程序
        exe_path = self._find_packaged_exe()
        if exe_path:
            self.step3_exe_found_signal.emit(exe_path)
            self._open_exe(exe_path)
        else:
            self.step3_exe_found_signal.emit("")
            self._log("WARNING", "未自动找到打包程序，请手动在资源管理器中打开")

        # 记录初始状态（可能在 Step 2 之前运行已有残留文件）
        initial_sizes = {}
        if rec_dir.exists():
            for f in rec_dir.glob("*.rec.upipelinecache"):
                initial_sizes[str(f)] = f.stat().st_size

        if initial_sizes:
            self._log("INFO", f"检测到已有 {len(initial_sizes)} 个 .rec 文件，将等待新变化...")

        POLL_INTERVAL = 2       # 检查间隔（秒）
        MAX_WAIT = 600          # 最多等待 600 秒（10 分钟）

        last_sizes = dict(initial_sizes)
        waited = 0
        has_detected_activity = bool(initial_sizes)  # 有初始文件也算活动

        while True:
            # 检查终止/跳过
            if self._stop_requested:
                self._log("WARNING", "用户中止等待，正在关闭游戏进程...")
                self.step3_auto_mode_signal.emit(False)
                self._terminate_game_process()
                self._add_step_detail(f"等待 {waited}s 后被用户中止")
                return False
            if self._step3_skip_wait:
                self._log("INFO", "用户手动跳过等待，正在关闭游戏进程...")
                self.step3_auto_mode_signal.emit(False)
                # 用户跳过 = 不再收集 PSO，关闭游戏进程释放资源
                self._terminate_game_process()
                self._add_step_detail(f"等待 {waited}s 后被用户跳过")
                self._print_summary(rec_dir)
                # 如果没有任何 .rec 文件，标记为失败，阻止后续无意义步骤
                has_any_rec = rec_dir.exists() and any(rec_dir.glob("*.rec.upipelinecache"))
                if not has_any_rec:
                    self._log("ERROR", "未收集到任何 PSO 记录，流程中断")
                return has_any_rec

            time.sleep(POLL_INTERVAL)
            waited += POLL_INTERVAL

            # 检查游戏进程是否已退出 —— 唯一的自动继续条件
            if self._game_process is not None and self._game_process.poll() is not None:
                exit_code = self._game_process.poll()
                self._game_process = None
                self.step3_game_running_signal.emit(False)
                self._log("INFO", f"检测到打包程序已退出 (exit code: {exit_code})")

                # 延迟 Tick 检测：游戏关闭后 PSO 文件可能延迟刷盘，
                # 最多等待 60 秒，轮询检测目录和文件是否已稳定写入
                POST_EXIT_MAX_WAIT = 60
                STABLE_CHECKS_NEEDED = 2  # 需要连续 2 次（4秒）文件大小不变才认为稳定
                post_exit_waited = 0
                stable_count = 0
                last_post_sizes = {}
                post_rec_found = False

                self._log("INFO", f"等待 PSO 文件写入完成（最多 {POST_EXIT_MAX_WAIT}s）...")

                while post_exit_waited < POST_EXIT_MAX_WAIT:
                    if self._stop_requested or self._step3_skip_wait:
                        break

                    time.sleep(POLL_INTERVAL)
                    post_exit_waited += POLL_INTERVAL
                    waited += POLL_INTERVAL

                    # 扫描当前文件状态
                    post_sizes = {}
                    if rec_dir.exists():
                        for f in rec_dir.glob("*.rec.upipelinecache"):
                            post_sizes[str(f)] = f.stat().st_size

                    if post_sizes:
                        post_rec_found = True
                        has_detected_activity = True
                        # 检查文件是否稳定（大小不再变化）
                        if post_sizes == last_post_sizes:
                            stable_count += 1
                            if stable_count >= STABLE_CHECKS_NEEDED:
                                total_kb = sum(post_sizes.values()) / 1024
                                self._log("SUCCESS",
                                          f"PSO 文件已稳定写入 "
                                          f"({len(post_sizes)} 个文件, {total_kb:.1f} KB)，自动继续")
                                break
                        else:
                            stable_count = 0
                            last_post_sizes = dict(post_sizes)
                            self._log("INFO",
                                      f"检测到 PSO 文件正在写入，等待稳定... "
                                      f"({len(post_sizes)} 个文件, "
                                      f"{sum(post_sizes.values()) / 1024:.1f} KB, "
                                      f"已等待 {post_exit_waited}s)")
                    else:
                        stable_count = 0
                        last_post_sizes = {}
                        if post_exit_waited % 10 == 0:
                            self._log("INFO",
                                      f"等待 PSO 目录/文件生成... "
                                      f"(已等待 {post_exit_waited}s/{POST_EXIT_MAX_WAIT}s)")

                # 如果用户中止/跳过，回到外层循环处理
                if self._stop_requested or self._step3_skip_wait:
                    continue

                # 自然超时或完成
                if post_rec_found:
                    if post_exit_waited >= POST_EXIT_MAX_WAIT:
                        self._log("WARNING",
                                  f"等待超时 ({POST_EXIT_MAX_WAIT}s)，"
                                  f"使用当前已有 {len(post_sizes)} 个 PSO 文件继续")
                    self._log("SUCCESS", "游戏已关闭，PSO 记录已收集，自动继续")
                else:
                    self._log("WARNING",
                              f"游戏已关闭但未检测到 PSO 记录活动 "
                              f"(等待 {post_exit_waited}s)")
                break

            # 扫描当前文件状态（仅日志记录，不作为结束条件）
            current_sizes = {}
            if rec_dir.exists():
                for f in rec_dir.glob("*.rec.upipelinecache"):
                    current_sizes[str(f)] = f.stat().st_size

            if not current_sizes:
                if waited % 10 == 0:
                    remaining = MAX_WAIT - waited
                    self._log("INFO", f"等待中... ({waited}s 已过, 剩余 {remaining}s) 请运行打包程序并遍历场景")
                last_sizes = {}
                if waited >= MAX_WAIT:
                    self._log("WARNING", f"等待超时 ({MAX_WAIT}s)，未检测到 .rec 文件")
                    self._add_step_detail("超时: 未检测到任何 .rec 文件")
                    break
                continue

            # 有文件 → 检测活动（仅日志）
            has_new = len(current_sizes) != len(last_sizes)
            has_size_change = False
            for path, size in current_sizes.items():
                old_size = last_sizes.get(path, -1)
                if old_size != size:
                    has_size_change = True
                    break

            file_active = has_new or has_size_change

            if file_active and not has_detected_activity:
                has_detected_activity = True
                total_size_kb = sum(current_sizes.values()) / 1024
                self._log("SUCCESS", f"检测到 PSO 记录活动！"
                          f" {len(current_sizes)} 个文件, 共 {total_size_kb:.1f} KB")

            if file_active:
                total_size_kb = sum(current_sizes.values()) / 1024
                self._log("INFO", f"PSO 文件有新变化 ({len(current_sizes)} 个, {total_size_kb:.1f} KB)")

            last_sizes = current_sizes

            # 超时检查
            if waited >= MAX_WAIT:
                self._log("WARNING", f"等待超时 ({MAX_WAIT}s)，使用当前已有文件继续")
                self._add_step_detail(f"等待 {waited}s 后超时")
                break

        self.step3_auto_mode_signal.emit(False)
        # PSO 收集阶段结束，关闭游戏进程
        self._terminate_game_process()
        self._add_step_detail(f"等待总耗时: {waited}s")

        # 将打包目录下的 .rec 文件同步到项目目录，供 Step 4/5 使用
        proj_rec_dir = Path(proj.resolve_rec_source_dir())
        if rec_dir != proj_rec_dir and rec_dir.exists():
            proj_rec_dir.mkdir(parents=True, exist_ok=True)
            copied = 0
            for f in rec_dir.glob("*.rec.upipelinecache"):
                shutil.copy2(f, proj_rec_dir / f.name)
                copied += 1
            if copied > 0:
                self._log("SUCCESS", f"已同步 {copied} 个 .rec 文件到项目目录: {proj_rec_dir}")
                self._add_step_detail(f"同步 {copied} 个 .rec 到 {proj_rec_dir}")

        self._print_summary(rec_dir)
        return True

    def _print_summary(self, rec_dir: Path):
        """打印最终收集结果摘要"""
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 3 收集结果：")
        if rec_dir.exists():
            rec_files = sorted(rec_dir.glob("*.rec.upipelinecache"))
            if rec_files:
                total_kb = sum(f.stat().st_size for f in rec_files) / 1024
                for f in rec_files:
                    size_kb = f.stat().st_size / 1024
                    self._log("SUCCESS", f"  {f.name}  ({size_kb:.1f} KB)")
                    self._add_step_detail(f"{f.name}  ({size_kb:.1f} KB)")
                self._log("SUCCESS", f"  共 {len(rec_files)} 个文件, 总计 {total_kb:.1f} KB")
                self._add_step_detail(f"收集: {len(rec_files)} 个 .rec 文件, 共 {total_kb:.1f} KB")
            else:
                self._log("WARNING", "  未收集到任何 .rec.upipelinecache 文件")
                self._add_step_detail("未收集到 .rec 文件")
        else:
            self._log("WARNING", f"  PSO 记录目录不存在: {rec_dir}")
            self._add_step_detail(f"目录不存在: {rec_dir}")

    # ---- Step 4: 确认 .shk 文件可用 ----

    def Step4_confirm_shk(self) -> bool:
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 4: 确认 .shk 稳定键文件可用")
        self._log("INFO", "=" * 50)

        if not self._project:
            return False

        shk_dir = Path(self._project.resolve_shk_source_dir())
        self._add_step_detail(f"SHK 目录: {shk_dir}")

        if not shk_dir.exists():
            self._log("ERROR", f".shk 目录不存在: {shk_dir}")
            self._log("INFO", "请确认 Step 2（首次打包）已成功完成")
            self._add_step_detail("目录不存在")
            return False

        shk_files = list(shk_dir.glob("*.shk"))
        if not shk_files:
            self._log("ERROR", f"目录下未找到 .shk 文件: {shk_dir}")
            self._add_step_detail("未找到 .shk 文件")
            return False

        shk_total = 0
        for f in shk_files:
            size_kb = f.stat().st_size / 1024
            self._log("SUCCESS", f"  找到: {f.name}  ({size_kb:.1f} KB)")
            self._add_step_detail(f"  {f.name}  ({size_kb:.1f} KB)")
            shk_total += size_kb

        self._add_step_detail(f"共 {len(shk_files)} 个 .shk, 总 {shk_total:.1f} KB")

        # 检查 .rec 文件
        rec_dir = Path(self._project.resolve_rec_source_dir())
        rec_files = list(rec_dir.glob("*.rec.upipelinecache")) if rec_dir.exists() else []

        if rec_files:
            rec_total = 0
            for f in rec_files:
                size_kb = f.stat().st_size / 1024
                self._log("SUCCESS", f"  找到: {f.name}  ({size_kb:.1f} KB)")
                self._add_step_detail(f"  {f.name}  ({size_kb:.1f} KB)")
                rec_total += size_kb
            self._add_step_detail(f"共 {len(rec_files)} 个 .rec, 总 {rec_total:.1f} KB")
        else:
            self._log("WARNING", f".rec 目录下未找到 .rec.upipelinecache 文件: {rec_dir}")
            self._log("INFO", "请确认 Step 3（收集 PSO）已正确完成")
            self._add_step_detail(f".rec 文件未找到  ({rec_dir})")

        return len(shk_files) > 0

    # ---- Step 5: 汇聚缓存文件 ----

    def Step5_gather_files(self) -> bool:
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 5: 汇聚 .rec + .shk 到 PSO 缓存工作目录")
        self._log("INFO", "=" * 50)

        if not self._project:
            return False

        work_dir = Path(self._project.pso_cache_work_dir)
        work_dir.mkdir(parents=True, exist_ok=True)
        self._add_step_detail(f"工作目录: {work_dir}")

        # 复制 .shk 文件
        shk_dir = Path(self._project.resolve_shk_source_dir())
        shk_copied = 0
        if shk_dir.exists():
            for f in shk_dir.glob("*.shk"):
                size_kb = f.stat().st_size / 1024
                dst = work_dir / f.name
                shutil.copy2(f, dst)
                self._log("SUCCESS", f"  复制: {f.name} -> {dst}")
                self._add_step_detail(f"  .shk: {f.name}  ({size_kb:.1f} KB)")
                shk_copied += 1

        if shk_copied == 0:
            self._log("ERROR", "未复制任何 .shk 文件")
            self._add_step_detail("未复制 .shk 文件")
            return False

        # 复制 .rec 文件
        rec_dir = Path(self._project.resolve_rec_source_dir())
        rec_copied = 0
        if rec_dir.exists():
            for f in rec_dir.glob("*.rec.upipelinecache"):
                size_kb = f.stat().st_size / 1024
                dst = work_dir / f.name
                shutil.copy2(f, dst)
                self._log("SUCCESS", f"  复制: {f.name} -> {dst}")
                self._add_step_detail(f"  .rec: {f.name}  ({size_kb:.1f} KB)")
                rec_copied += 1

        if rec_copied == 0:
            self._log("WARNING", "未复制任何 .rec.upipelinecache 文件（可能尚未收集 PSO）")
            self._add_step_detail("未复制 .rec 文件")
        else:
            self._add_step_detail(f".shk: {shk_copied} 个 | .rec: {rec_copied} 个")

        self._log("SUCCESS", f"汇聚完成：{shk_copied} 个 .shk + {rec_copied} 个 .rec")
        return True

    # ---- Step 6: 转换缓存（expand → .spc） ----

    def Step6_expand_spc(self) -> bool:
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 6: 转换缓存 — ShaderPipelineCacheTools expand (.spc)")
        self._log("INFO", "=" * 50)

        if not self._project:
            self._log("ERROR", "未选择项目，请先在顶部下拉框选择项目")
            return False
        if not self._ue5:
            self._log("ERROR",
                f"项目「{self._project.name}」关联的 UE 版本「{self._project.ue5_version}」"
                "未在配置中设置，请切换到「配置管理」页面检查 UE 引擎配置")
            return False

        work_dir = Path(self._project.pso_cache_work_dir)
        self._add_step_detail(f"工作目录: {work_dir}")

        if not work_dir.exists():
            self._log("ERROR", f"PSO 缓存工作目录不存在: {work_dir}")
            self._add_step_detail("工作目录不存在")
            return False

        # 找到 .rec 和 .shk 文件
        rec_files = list(work_dir.glob("*.rec.upipelinecache"))
        shk_files = list(work_dir.glob("*.shk"))

        if not rec_files:
            self._log("ERROR", "工作目录中未找到 .rec.upipelinecache 文件")
            self._add_step_detail("未找到 .rec.upipelinecache")
            return False
        if not shk_files:
            self._log("ERROR", "工作目录中未找到 .shk 文件")
            self._add_step_detail("未找到 .shk")
            return False

        # 使用通配符，确保所有 .rec 和 .shk 文件（多格式）都被 expand 使用
        # 否则只取第一个文件的 shader format 会导致其他格式的 shader hash 全部不匹配
        rec_pattern = str(work_dir / "*.rec.upipelinecache")
        shk_pattern = str(work_dir / "*.shk")
        spc_output = str(work_dir / "PipelineCache.spc")
        uproject = self._project.uproject_file

        self._add_step_detail(f"输入 REC: *.rec.upipelinecache ({len(rec_files)} 个)")
        self._add_step_detail(f"输入 SHK: *.shk ({len(shk_files)} 个)")
        self._add_step_detail(f"输出 SPC: PipelineCache.spc")

        self._log("INFO", f"  REC: {len(rec_files)} 个 .rec.upipelinecache")
        for sf in shk_files:
            self._log("INFO", f"  SHK: {sf.name}")
        self._log("INFO", f"  输出: PipelineCache.spc")

        cmd = (
            f'"{self._ue5.editor_cmd_path}"'
            f' "{uproject}"'
            f' -run=ShaderPipelineCacheTools'
            f' expand'
            f' "{rec_pattern}"'
            f' "{shk_pattern}"'
            f' "{spc_output}"'
            f' -NullRHI -unattended'
        )

        ok, output = self._run_cmd(cmd)

        # 检测 ShaderPipelineCacheTools expand 是否生成了 0 个 stable PSO
        if "Generated 0 stable PSOs" in output or "No stable PSOs created" in output:
            self._log("ERROR", "ShaderPipelineCacheTools expand 生成了 0 个 stable PSO！")
            self._log("ERROR", "原因：.rec 中的 Shader Hash 与 .shk 不匹配")
            self._log("ERROR", "通常因为 .rec 和 .shk 来自不同构建，或 Cook 缓存不一致")
            self._log("INFO", "建议：确保 Step 2 完全重新 Cook（已自动清理 Saved/Cooked），然后重新运行 Step 3 收集 PSO")
            self._add_step_detail("0 stable PSO - .rec/.shk 不匹配")
            return False

        if ok:
            if Path(spc_output).exists():
                size_kb = Path(spc_output).stat().st_size / 1024
                self._log("SUCCESS", f".spc 文件已生成: {Path(spc_output).name} ({size_kb:.1f} KB)")
                self._add_step_detail(f"生成: PipelineCache.spc  ({size_kb:.1f} KB)")
            else:
                self._log("ERROR", ".spc 文件未生成，命令可能未成功")
                self._add_step_detail("SPC 未生成")
                return False

        return ok

    # ---- Step 7: 集成 .spc 到 Build 目录 ----

    def Step7_integrate_spc(self) -> bool:
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 7: 集成 .spc 到 Build/Windows/PipelineCaches/")
        self._log("INFO", "=" * 50)

        if not self._project:
            return False

        work_dir = Path(self._project.pso_cache_work_dir)
        spc_files = list(work_dir.glob("PipelineCache.spc"))
        if not spc_files:
            self._log("ERROR", f"工作目录中未找到 .spc 文件: {work_dir}")
            self._add_step_detail("未找到 PipelineCache.spc")
            return False

        spc_src = spc_files[0]
        target_dir = Path(self._project.resolve_spc_target_dir())
        target_dir.mkdir(parents=True, exist_ok=True)

        self._add_step_detail(f"源: {spc_src}")
        self._add_step_detail(f"目标: {target_dir}")

        # 复制 .spc 到 Build 目录
        dst = target_dir / spc_src.name
        shutil.copy2(spc_src, dst)
        self._log("SUCCESS", f"  复制: {spc_src.name} -> {dst}")
        self._add_step_detail(f"已复制 PipelineCache.spc")

        return True

    # ---- Step 8: 最终打包 ----

    def Step8_final_build(self) -> bool:
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 8: 最终打包 — PSO 缓存入包")
        self._log("INFO", "=" * 50)

        if self._project:
            self._add_step_detail(f"项目: {self._project.name}")
            self._add_step_detail(f"平台: {self._project.target_platform}")
            self._add_step_detail(f"输出: {self._project.output_dir}")
            self._add_step_detail("PSO 缓存将内建入包")

        return self._run_uat_build("Step 8 最终打包")

    # ---- Step 9: 测试 PSO 覆盖范围 ----

    def Step9_test_pso_coverage(self) -> bool:
        self._log("INFO", "=" * 50)
        self._log("INFO", "Step 9: 测试 PSO 覆盖范围 — 运行打包程序并分析日志")
        self._log("INFO", "=" * 50)

        if not self._project:
            return False

        proj = self._project
        exe_path = self._find_packaged_exe()
        if not exe_path:
            self._log("ERROR", "未找到打包程序 (.exe)，请确认 Step 8 最终打包已完成")
            self._add_step_detail("未找到打包程序")
            return False

        self._add_step_detail(f"程序: {exe_path}")

        # 日志文件路径
        log_dir = Path(proj.output_dir) / "Windows" / proj.name / "Saved" / "Logs"
        log_dir.mkdir(parents=True, exist_ok=True)
        log_file = log_dir / f"{proj.name}_PSOTest.log"

        self._add_step_detail(f"日志: {log_file}")

        self._step9_request_close = False

        self._log("INFO", "")
        self._log("INFO", "启动打包程序（带 -logpso 参数），请在游戏中遍历所有场景...")
        self._log("INFO", "测试完成后正常退出游戏即可，工具会自动分析日志")
        self._log("INFO", "")

        # 使用 -logpso 启动游戏
        try:
            exe_dir = Path(exe_path).parent
            exe_name = Path(exe_path).name

            cmd = f'"{exe_path}" -logpso -log="{log_file}"'
            self._log("INFO", f">> {cmd}")

            self._game_process = subprocess.Popen(
                f'"{exe_path}" -logpso -log="{log_file}"',
                cwd=str(exe_dir),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            self._game_exe_path = exe_path
            self._log("SUCCESS", f"已启动游戏 (PID: {self._game_process.pid})，等待退出...")
            self.step9_game_running_signal.emit(True)

            self.step9_panel_signal.emit(True)

            # 等待游戏退出
            MAX_WAIT = 60  # 最多等 1 分钟
            POLL_INTERVAL = 2
            waited = 0

            while self._game_process and self._game_process.poll() is None:
                if self._stop_requested or self._step9_request_close:
                    reason = "用户中止" if self._stop_requested else "用户请求关闭"
                    self._log("WARNING", f"{reason}，正在关闭游戏...")
                    self._terminate_game_process()
                    self.step9_game_running_signal.emit(False)
                    self.step9_panel_signal.emit(False)
                    if self._stop_requested:
                        return False
                    self._step9_request_close = False
                    break
                time.sleep(POLL_INTERVAL)
                waited += POLL_INTERVAL
                if waited % 30 == 0:
                    self._log("INFO", f"等待游戏退出... ({waited}s)")

                if waited >= MAX_WAIT:
                    self._log("WARNING", f"等待超时 ({MAX_WAIT}s)，强制关闭游戏")
                    self._terminate_game_process()
                    break

            exit_code = self._game_process.poll() if self._game_process else None
            self._game_process = None
            self.step9_game_running_signal.emit(False)
            self.step9_panel_signal.emit(False)

            # 延迟等待日志刷盘
            self._log("INFO", "游戏已退出，等待日志文件写入...")
            time.sleep(3)

        except Exception as e:
            self._log("ERROR", f"启动游戏失败: {e}")
            self.step9_panel_signal.emit(False)
            return False

        # 检查日志文件是否存在
        if not log_file.exists():
            self._log("WARNING", f"指定日志文件未找到: {log_file}")
            self._log("INFO", "尝试查找最新的日志文件...")
            # 查找 Saved/Logs 下最新的 .log 文件
            if log_dir.exists():
                all_logs = sorted(log_dir.glob("*.log"), key=lambda f: f.stat().st_mtime, reverse=True)
                if all_logs:
                    log_file = all_logs[0]
                    self._log("SUCCESS", f"使用最新日志: {log_file}")
                else:
                    self._log("ERROR", f"日志目录下未找到任何 .log 文件: {log_dir}")
                    self._add_step_detail("未找到日志文件")
                    return False
            else:
                self._log("ERROR", f"日志目录不存在: {log_dir}")
                self._add_step_detail("日志目录不存在")
                return False

        self._add_step_detail(f"分析日志: {log_file.name}")

        # 解析日志
        try:
            log_content = log_file.read_text(encoding="utf-8", errors="replace")
        except Exception as e:
            self._log("ERROR", f"读取日志文件失败: {e}")
            self._add_step_detail(f"读取日志失败: {e}")
            return False

        # 提取关键指标
        cache_entries = 0
        cache_guid = ""
        new_pso_count = 0
        new_pso_list: list[str] = []
        total_saved = 0
        total_new_saved = 0
        pso_hit_count = 0
        pso_miss_count = 0

        for line in log_content.splitlines():
            # "Opened FPipelineCacheFile: ... with N entries"
            m = re.search(r'Opened FPipelineCacheFile:.*?with\s+(\d+)\s+entries', line)
            if m:
                cache_entries = max(cache_entries, int(m.group(1)))
                guid_m = re.search(r'GUID:\s*([0-9A-Fa-f]+)', line)
                if guid_m:
                    cache_guid = guid_m.group(1)

            # "Encountered a new graphics PSO: N"
            m = re.search(r'Encountered a new graphics PSO:\s*(\d+)', line)
            if m:
                new_pso_count += 1
                new_pso_list.append(m.group(1))

            # "saved N total, M new, K removed"
            m = re.search(r'saved\s+(\d+)\s+total,\s+(\d+)\s+new', line)
            if m:
                total_saved = max(total_saved, int(m.group(1)))
                total_new_saved = max(total_new_saved, int(m.group(2)))

            # PSO hit/miss
            if re.search(r'PSO\s*[:]?\s*Hit', line, re.IGNORECASE):
                pso_hit_count += 1
            if re.search(r'PSO\s*[:]?\s*Miss', line, re.IGNORECASE):
                pso_miss_count += 1

        # 计算覆盖率
        total_at_exit = max(total_saved, cache_entries + new_pso_count)
        if total_at_exit > 0:
            coverage_pct = (cache_entries / total_at_exit) * 100
        else:
            coverage_pct = 0.0
            total_at_exit = cache_entries + new_pso_count

        # 如果 total_saved 为 0，尝试用退出时另有记录推断
        if total_saved == 0:
            total_saved = total_at_exit

        coverage_pct = (cache_entries / total_saved * 100) if total_saved > 0 else 0.0

        # 评估等级
        if coverage_pct >= 95:
            grade = "🟢 优秀"
            grade_color = "#4ECB71"
            advice = "PSO 缓存覆盖率充足，运行时几乎不会产生着色器编译卡顿"
        elif coverage_pct >= 70:
            grade = "🟡 一般"
            grade_color = "#F0C040"
            advice = "覆盖率中等，建议在更多场景/条件下重复 Step 3 收集 PSO，再重新执行 Step 4-8"
        elif coverage_pct > 0:
            grade = "🔴 不足"
            grade_color = "#E0556A"
            advice = "覆盖率严重不足！请确认 Step 3 充分遍历了所有场景和可扩展性设置，重新执行完整流程"
        else:
            grade = "⚪ 无覆盖"
            grade_color = "#888888"
            advice = "预编译缓存未生效，请检查 Step 6-8 是否成功生成了稳定 PSO 缓存"

        # 输出详细报告
        self._log("INFO", "")
        self._log("INFO", "=" * 60)
        self._log("INFO", "  PSO 覆盖范围测试报告")
        self._log("INFO", "=" * 60)

        self._log("INFO", f"  日志文件:      {log_file.name}")
        self._log("INFO", f"  文件大小:      {log_file.stat().st_size / 1024:.1f} KB")
        self._log("INFO", "-" * 60)
        self._log("INFO", f"  预编译缓存条目:  {cache_entries}")
        if cache_guid:
            self._log("INFO", f"  缓存 GUID:       {cache_guid}")
        self._log("INFO", f"  运行时新 PSO:    {new_pso_count}")
        self._log("INFO", f"  退出时 PSO 总数: {total_saved}")
        self._log("INFO", "-" * 60)
        self._log("INFO", f"  覆盖率:          {coverage_pct:.1f}%")
        self._log("INFO", f"  评估:            {grade}")
        self._log("INFO", f"  建议:            {advice}")

        # 显示前 10 个新 PSO 的 Hash
        if new_pso_list:
            shown = new_pso_list[:10]
            self._log("INFO", f"  新 PSO Hash (前 {len(shown)} 个):")
            for h in shown:
                self._log("INFO", f"    - {h}")
            if len(new_pso_list) > 10:
                self._log("INFO", f"    ... 还有 {len(new_pso_list) - 10} 个")

        self._log("INFO", "=" * 60)

        # 填充详情
        self._add_step_detail(f"预编译缓存条目: {cache_entries}")
        self._add_step_detail(f"运行时新 PSO: {new_pso_count}")
        self._add_step_detail(f"退出时 PSO 总数: {total_saved}")
        self._add_step_detail(f"覆盖率: {coverage_pct:.1f}%")
        self._add_step_detail(f"评估: {grade}")
        self._add_step_detail(advice)

        # 发射 PSO 覆盖数据到 UI 面板
        self.pso_coverage_data.emit({
            "cache_entries": cache_entries,
            "cache_guid": cache_guid,
            "new_pso_count": new_pso_count,
            "new_pso_list": new_pso_list[:10],
            "total_saved": total_saved,
            "total_new_saved": total_new_saved,
            "coverage_pct": coverage_pct,
            "grade": grade,
            "advice": advice,
            "log_file_name": log_file.name,
            "log_file_size_kb": log_file.stat().st_size / 1024,
        })

        # 不够好的情况视为"成功但有问题"
        if coverage_pct >= 70:
            self._log("SUCCESS", f"PSO 覆盖测试完成 — 覆盖率 {coverage_pct:.1f}% ({grade})")
        else:
            self._log("WARNING", f"PSO 覆盖率偏低: {coverage_pct:.1f}% ({grade})")
            # 不标为失败，因为工具本身没有错误
            self._log("INFO", "提示: 覆盖率不足不代表工具出错，建议重新收集 PSO 并重新打包")

        return True

    # ---- UAT 打包核心逻辑 ----

    def _run_uat_build(self, label: str) -> bool:
        """调用 RunUAT.bat BuildCookRun 进行打包"""
        if not self._project:
            self._log("ERROR", "未选择项目，请先在顶部下拉框选择项目")
            return False
        if not self._ue5:
            self._log("ERROR",
                f"项目「{self._project.name}」关联的 UE 版本「{self._project.ue5_version}」"
                "未在配置中设置，请切换到「配置管理」页面检查 UE 引擎配置")
            return False

        # 打包前确保之前启动的游戏进程已关闭，防止 exe 文件被占用
        self._ensure_game_closed()

        uproject = self._project.uproject_file
        output_dir = self._project.output_dir
        platform = self._project.target_platform
        shader_formats = "+".join(self._project.shader_formats)

        cmd = (
            f'"{self._ue5.uat_bat_path}"'
            f' BuildCookRun'
            f' -project="{uproject}"'
            f' -platform={platform}'
            f' -clientconfig=Development'
            f' -build'
            f' -cook'
            f' -stage'
            f' -pak'
            f' -archive'
            f' -archivedirectory="{output_dir}"'
            f' -unrealexe="{self._ue5.editor_cmd_path}"'
            f' -utf8output'
            f' -compile'
            f' -CrashReporter'
            f' -ShaderFormats={shader_formats}'
        )

        self._log("INFO", f"开始 {label}...")
        self._log("INFO", f"  项目: {self._project.name}")
        self._log("INFO", f"  输出: {output_dir}")

        ok, _ = self._run_cmd(cmd)
        if ok:
            self._log("SUCCESS", f"{label} 完成 ✓")
        else:
            self._log("ERROR", f"{label} 失败")

        return ok
