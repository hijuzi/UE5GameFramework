"""
步骤配置定义 - StepConfig 数据类
描述 PSO 打包工作流中的 10 个步骤
"""

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Optional


class StepStatus(Enum):
    PENDING = auto()     # 待执行
    RUNNING = auto()     # 执行中
    SUCCESS = auto()     # 成功
    FAILED = auto()      # 失败
    SKIPPED = auto()     # 已跳过


@dataclass
class StepConfig:
    """单个步骤的配置定义"""
    index: int                          # 步骤序号 0-8
    name: str                           # 步骤名称
    description: str                    # 步骤详细描述
    estimated_time: str                 # 预估耗时
    depends_on: list[int] = field(default_factory=list)  # 依赖的前置步骤序号


# PSO 打包工作流 9 步定义
PSO_WORKFLOW_STEPS: list[StepConfig] = [
    StepConfig(
        index=0,
        name="验证配置",
        description="加载并验证 config.json 完整性，检查 UE 引擎路径和项目路径是否存在",
        estimated_time="< 5秒",
        depends_on=[],
    ),
    StepConfig(
        index=1,
        name="检查 PSO 项目配置",
        description="检查 DefaultEngine.ini 和 DefaultGame.ini 中的 PSO 相关配置项是否完备",
        estimated_time="< 5秒",
        depends_on=[0],
    ),
    StepConfig(
        index=2,
        name="首次打包（生成 .shk）",
        description="调用 UAT BuildCookRun 进行首次打包，生成 Shader 稳定键文件（.shk）",
        estimated_time="10-60 分钟",
        depends_on=[1],
    ),
    StepConfig(
        index=3,
        name="收集 PSO 记录",
        description="运行打包程序自动遍历场景收集 PSO 缓存；可选参数 -psosysautocoverage 自动开始覆盖采集、-psosysautoquitgame 采集完成后自动退出游戏",
        estimated_time="5-30 分钟（自动采集）",
        depends_on=[2],
    ),
    StepConfig(
        index=4,
        name="确认 .shk 文件",
        description="确认步骤2生成的 .shk 稳定键文件可用且未被覆盖",
        estimated_time="< 5秒",
        depends_on=[3],
    ),
    StepConfig(
        index=5,
        name="汇聚缓存文件",
        description="将 .rec.upipelinecache 和 .shk 文件复制到 PSO 缓存工作目录",
        estimated_time="< 10秒",
        depends_on=[4],
    ),
    StepConfig(
        index=6,
        name="转换缓存（生成 .spc）",
        description="调用 ShaderPipelineCacheTools expand 将 .rec + .shk 转换为 .spc 文件",
        estimated_time="2-10 分钟",
        depends_on=[5],
    ),
    StepConfig(
        index=7,
        name="集成 .spc 到 Build",
        description="将生成的 .spc 文件复制到 Build/Windows/PipelineCaches/ 目录",
        estimated_time="< 5秒",
        depends_on=[6],
    ),
    StepConfig(
        index=8,
        name="最终打包（PSO 入包）",
        description="再次调用 UAT BuildCookRun，将 .stable.upipelinecache 打入最终包体",
        estimated_time="10-60 分钟",
        depends_on=[7],
    ),
    StepConfig(
        index=9,
        name="测试 PSO 覆盖范围",
        description="使用 -logpso 参数运行打包程序，解析日志输出 PSO 缓存命中率、预编译条目数、运行时新 PSO 数量等详细覆盖报告",
        estimated_time="3-15 分钟（自动分析）",
        depends_on=[8],
    ),
]
