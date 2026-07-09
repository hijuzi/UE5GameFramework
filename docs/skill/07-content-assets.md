# 资产与内容技能

资产的引用与加载、数据驱动设计、材质着色器、静态/骨骼网格和内容导入管线。

---

## asset-management

**适用于**：正确引用和加载 UE 资产 —— 硬引用 vs 软引用（TObjectPtr vs TSoftObjectPtr vs TSoftClassPtr）；同步/异步加载；Asset Manager 与 Primary Asset；引用查看器（Reference Viewer）。

**核心内容**：硬引用（UPROPERTY TObjectPtr<T>，加载时立即拉入内存）vs 软引用（TSoftObjectPtr<T>/TSoftClassPtr<T>，按需加载）；StreamableManager 异步加载；FStreamableHandle；Primary Asset Id / Primary Asset Type；Asset Bundle 打包优化；Asset Registry 元数据查询。

---

## data-driven-design

**适用于**：用外部数据驱动游戏逻辑替代硬编码 —— DataTable（FTableRowBase）；DataAsset（UDataAsset）；CurveTable（FRichCurve）；Data Registry。

**核心内容**：UDataTable（CSV/JSON 导入 → FTableRowBase 子类行结构）；UDataAsset（编辑器内配置数据，支持继承）；UCurveTable（FRichCurve Float/Vector 曲线）；UCurveFloat/UCurveVector（曲线资产）；Data Registry 全局数据源；GetDataTableRow/FindRow 查找函数。

---

## materials-and-shaders

**适用于**：制作和驱动材质 —— UMaterial（节点图资产）/ MaterialInstance（参数化变体）；Material Expression 节点；Material Parameter Collection（全局参数）；Material Function；Custom HLSL 节点。

**核心内容**：Material Domain（Surface / DeferredDecal / LightFunction / PostProcess）；Blend Mode（Opaque / Masked / Translucent / Additive / Modulate）；Material Parameter（Scalar/Vector/Texture 参数）→ Material Instance Dynamic（MID）运行时修改；Material Parameter Collection（MPC）全局参数共享；Material Function 封装复用；If/Switch 材质节点的性能代价。

---

## meshes-static-and-skeletal

**适用于**：操作静态和骨骼网格体 —— UStaticMesh + UStaticMeshComponent；USkeletalMesh + USkeletalMeshComponent；LOD 系统；Socket 挂接点。

**核心内容**：StaticMesh LOD（LODGroup / ScreenSize / 手动配置）；SkeletalMesh LOD（LODInfo / Reduction Settings / BonesToRemove）；Socket（静态/骨骼网格挂接武器/特效）；UStaticMeshComponent::SetStaticMesh 运行时替换；USkeletalMeshComponent::SetSkeletalMesh；Skeletal Mesh Merge（合并多个骨骼网格）。

---

## importing-content

**适用于**：将外部资产导入 UE —— Interchange 框架（UInterchangeManager / UInterchangeTranslatorBase / UInterchangePipelineBase）；FBX/glTF/USD 导入管道；Python 批量导入。

**核心内容**：Interchange 架构（SourceData → Translator → Pipeline → Factory → Asset）；Interchange Manager 导入入口；Interchange Pipeline 配置（网格合并/材质实例/骨骼网格 LOD）；FBX/glTF/USD Import；Python 批量导入脚本（ImportAssetTask）；导入设置项目默认值。
