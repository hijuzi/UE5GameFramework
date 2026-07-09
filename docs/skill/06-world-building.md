# 场景世界技能

关卡结构与流式加载、地形植被、光照系统（Lumen）和 Nanite 虚拟化几何。

---

## levels-and-world-partition

**适用于**：构建和流式加载 UE 关卡 —— UWorld（Persistent Level + Streaming Levels）；World Partition 大世界管理；关卡流式加载（体积/距离/蓝图控制）。

**核心内容**：UWorld = PersistentLevel + StreamingLevels；World Partition（Grid Cell / Data Layer / HLOD）；Streaming Source（Player Controlled）；Level Streaming Volume 体积触发；蓝图 LoadStreamLevel/UnloadStreamLevel；Level Instance（动态加载子关卡）；OFPA（One File Per Actor）。

---

## landscape-and-foliage

**适用于**：地形编辑、实例化植被和程序化环境生成 —— Landscape（高度图/图层）；Foliage（FoliageType + InstancedStaticMesh）；Grass Map 程序化植被；Landscape Material 图层混合。

**核心内容**：ALandscape（Heightmap / LayerWeight / Hole Material）；Landscape Layer Blend 节点；FoliageType（StaticMesh/VFX/Sound 的植被代理）；Procedural Foliage Spawner（基于 Landscape 层和坡度密度）；Grass Type + Landscape Grass Output 节点；HISM（Hierarchical Instanced Static Mesh）。

---

## lighting-and-lumen

**适用于**：配置光照 —— 光源组件（Directional/Point/Spot/SkyLight）；Lumen 全局光照；静态/固定/动态光照策略；PostProcessVolume 后处理。

**核心内容**：光源类型（DirectionalLight/PointLight/SpotLight/SkyLight/RectLight）；Mobility（Static/Stationary/Movable）；Lumen Global Illumination（软件光追 GI）；Lumen Reflections；Lightmass 静态烘焙；IES Profile 光照轮廓；Exponential Height Fog；PostProcessVolume（Bloom/ToneMapping/ColorGrading/DOF/MotionBlur）。

---

## nanite-and-rendering

**适用于**：配置 Nanite 虚拟化几何体 —— FMeshNaniteSettings（在 Static 和 Skeletal Mesh 上启用）；Nanite Fallback Mesh；Nanite 与 Lumen 配合；Virtual Shadow Maps。

**核心内容**：Nanite Enable/Disable；Nanite Fallback Mesh（远距离 LOD）；Nanite Overdraw 可视化；Virtual Shadow Maps（与 Nanite 配合）；Nanite 兼容性检查（Opaque/WorldPositionOffset/非 WPO 材质）；Preserve Area 设置；Nanite Tessellation。
