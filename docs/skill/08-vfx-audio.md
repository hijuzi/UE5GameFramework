# 视效与音频技能

音频系统（含 MetaSounds）和 Niagara 粒子特效系统。

---

## audio-and-metasounds

**适用于**：播放和控制音频 —— SoundWave / SoundCue / MetaSounds（SoundSource）；Audio Component 播放控制；衰减（Attenuation）；3D 空间化；Submix 效果链。

**核心内容**：音频资产类型（SoundWave-原始音频 / SoundCue-节点编辑器 / MetaSounds-程序化音频）；UAudioComponent 播放/停止/音量/音高控制；衰减设置（Attenuation Override / Attenuation Shape / Distance Algorithm）；Submix（Master/自定义 Submix）效果链（ EQ/Delay/Reverb/Compressor）；Sound Class（分层音量控制）；SynthComponent 程序合成。

---

## niagara-vfx

**适用于**：创建和控制 Niagara 视觉效果 —— UNiagaraSystem（效果资产）、UNiagaraComponent（效果实例）；Niagara 发射器/模块/参数；Data Interface（数据接口）；蓝图中控制 Niagara。

**核心内容**：Niagara System vs Emitter 层级；模块化设计（Spawn / Update / Render 阶段）；Parameter（User Exposed / System / Emitter / Particle）；Data Interface（SkeletalMesh / Texture / Audio / NeighborGrid3D）；Blueprint Niagara Function Node（SetVariable/GetVariable）；Pool Method（FreeInPool/Auto/None）；GPU Simulation（GPUComputeSimulation 标记）；Ribbon/Sprite/Mesh Renderer。
