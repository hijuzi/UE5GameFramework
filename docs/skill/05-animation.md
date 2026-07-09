# 动画系统技能

骨骼动画、程序化 IK 和过场电影系统。

---

## animation-system

**适用于**：使用 AnimInstance / Animation Blueprint 驱动骨骼动画；状态机（State Machine）；混合空间（BlendSpace）；动画通知（AnimNotify/AnimNotifyState）；动画曲线（AnimationCurve）；动画层接口。

**核心内容**：UAnimInstance（动画蓝图 C++ 父类）；AnimGraph + EventGraph 双图架构；状态机构建（State + Transition + TransitionRule）；BlendSpace 1D/2D；AimOffset；Montage（播放/暂停/BlendOut）；AnimNotify / AnimNotifyState（可执行 C++ 回调）；SyncGroup 同步标记；IK Rig / Foot IK；Linked Anim Graph（动画层级复用）。

---

## control-rig-and-ik

**适用于**：程序化动画和反向运动学（IK）—— Control Rig 资产；Full-Body IK；Sequencer 驱动 Control Rig；FK/IK 混合。

**核心内容**：Control Rig 资产创建与 Rig Graph；控件类型（Transform/Vector/Float）；RigUnit 节点库；Forward Solve（FK）和 Backward Solve（IK）；Full-Body IK Solver；Sequencer 中 Keyframe Control Rig；Control Rig 与 Anim Blueprint 的数据通道。

---

## sequencer-and-cinematics

**适用于**：从 C++ 创建和驱动过场电影 —— ULevelSequence（过场序列资产）； Sequencer 轨道（Actor/Transform/Event/Subscene）；Movie Scene 绑定；在游戏逻辑中播放 Sequence；Sequence Player 生命周期。

**核心内容**：ULevelSequence 资产加载；ALevelSequenceActor 生成；ULevelSequencePlayer 播放控制（Play/Pause/Stop/SetPlaybackPosition）；FMovieSceneSequencePlaybackParams；Sequence Event Track（在 C++/BP 中接收事件）；Subscene 嵌套；Master Sequence 与镜头切换。
