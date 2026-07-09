# Ultra Dynamic Sky (UDS) 技能

动态天空与大气渲染系统 —— 涵盖云层、雾效、光照、时间系统、天体模拟、过场渲染和性能调优。

---

## uds-setup-and-modes

**适用于**：将 UDS 安装到关卡并选择顶级模式 —— Sky Mode（Volumetric/Static/2D/Voxel/Aurora/Space）、Color Mode（Sky Atmosphere vs Simplified）、Project Mode（Game vs Cinematic）、Feature Level（Desktop/Mobile）。

**核心内容**：UDS Actor 添加方式；Sky Mode 选择（Volumetric Cloud / Static Cloud / 2D Dynamic / Voxel / Aurora Only / Space Only）；Color Mode（Sky Atmosphere 真实大气散射 vs Simplified 简化色）；Project Mode（Game 为运行时优化 vs Cinematic 为 Movie Render Queue 路径追踪优化）；Feature Level 移动端适配。

---

## uds-time

**适用于**：控制时间 —— 设置/动画化当前时间；昼夜循环；运行时 Set Time with Time Code / Set Time of Day with String / Transition Time of Day；获取函数（Get Time of Day / Get Time Code / Get Date Time）；事件分发器（Sunrise / Sunset / Midnight / Hourly / Every Minute）。

**核心内容**：Time of Day 控制（0-24 小时映射）；Day Length / Night Length 分别控制；Transition Time of Day（平滑过渡）；Get Time Code / Get Time of Day / Get Date Time 查询函数；Sunrise / Sunset / Midnight / Hourly / Current Hour Changed / Every Minute / Custom Time 事件分发器。

---

## uds-simulation

**适用于**：使用真实天文模拟 —— 真实太阳/月亮/星星模拟；纬度/经度/时区设置；城市预设；模拟日期；Simulation Speed；同步系统时间（Use System Time）。

**核心内容**：Simulate Real Sun/Moon/Stars（基于地理坐标的天体位置计算）；Latitude/Longitude/Time Zone 配置；North Yaw（指北方向）；Daylight Savings 夏令时处理；City Presets（预置城市经纬度）；Simulation Date；Simulation Speed（时间流速倍数）；Use System Time（同步操作系统时钟）。

---

## uds-sun-moon-stars

**适用于**：控制太阳、月亮、星星、极光、天空辉光和太空层（行星/月亮/星云）的视觉表现。

**核心内容**：太阳/月亮路径（Yaw/Pitch/Vertical Offset/Moon Orbit Offset）；手动定位（Sun/Moon Target Widget）；太阳外观（Scale/Softness/Color/Eclipse）；月亮相位和外观；星星（Tiling 平铺 vs 360 全天天体图）；2D/Volumetric Aurora（极光）；Night Sky Glow + Light Pollution（光污染控制）；Space Layer（添加行星/月亮/星云）。

---

## uds-clouds

**适用于**：配置云层 —— 体积云/静态云/2D 动态云/Voxel 云；Cloud Movement / Cloud Wisps；体积云绘制器；体积云光线；Cloud Profile Authoring Tool。

**核心内容**：Volumetric Cloud（真实体积渲染云）；Static Cloud（性能友好静态纹理云）；2D Dynamic Cloud（带动态移动的 2D 云）；Voxel Cloud（体素云）；Cloud Movement（方向/速度）；Cloud Wisps（高海拔卷云纹理）；Volumetric Cloud Painter（绘制云覆盖度）；Cloud Light Rays（体积云光线效果）；Cloud Profile 编辑工具。

---

## uds-fog-and-atmosphere

**适用于**：配置雾、体积雾、尘埃和大气着色 —— Fog Density（基础 + 云/雾/尘贡献、高度衰减、起始距离）；Fog Color；体积雾光散射；Global Volumetric Material（3D 噪声、地面雾、水面雾）；Dust；Sky Atmosphere 设置（Rayleigh 散射/吸收/阴天亮度）；Simplified Color 模式。

**核心内容**：Fog Density（基础密度 + 云/雾/尘分别贡献 + 高度 Falloff + 起始距离）；Fog Color 着色；Support Sky Atmosphere Affecting Height Fog 项目设置；Volumetric Fog（光散射）；Global Volumetric Material（3D Noise / Ground Fog / Water-Level Fog）；Dust 尘埃参数；Sky Atmosphere（Rayleigh Scattering/Absorption/Overcast Luminance）。

---

## uds-lighting-and-shadows

**适用于**：配置光照 —— 太阳和月亮方向光组件；云阴影（体积云阴影/2D 阴影）；天空光模式（Capture Based / Custom Cubemap / Cubemap with Dynamic Color Tinting）；自动/手动曝光；Screen Space Light Shafts；Light Day/Night Toggle（根据时间自动开关灯）；昼夜材质工具函数。

**核心内容**：太阳/月亮 DirectionalLight 组件参数；Cloud Shadows（体积云阴影 / 2D 云阴影）；Sky Light Mode 选择与性能（Capture Based / Cubemap / Cubemap + Dynamic Tint）；Exposure（Auto / Manual）；静态/固定光照设置；Screen Space Light Shafts；Light Day/Night Toggle（夜晚自动亮灯组件）；Day-to-Night 材质工具函数。

---

## uds-cinematics-rendering

**适用于**：UDS 的过场和渲染用途 —— 在 Sequencer 中关键帧驱动 UDS（Time of Day / Cloud Coverage / Fog / Cloud Movement 等）；Movie Render Queue 渲染（Project Mode Cinematic / Offline）；Path Tracer 支持（Adjust for Path Tracer、后处理高度雾近似）；体积云无缝循环。

**核心内容**：Sequencer 中关键帧 UDS 参数（Time of Day / Cloud Coverage / Fog / Cloud Movement / 额外变量）；Movie Render Queue（MRQ）配置（Project Mode 选 Cinematic/Offline）；Path Tracer 兼容（Adjust for Path Tracer、Post-Process Height Fog Approximation）；Volumetric Cloud 无缝循环配置。

---

## uds-modifiers-configs-state

**适用于**：使用 UDS 的高级状态功能 —— Sky Modifier（数据资产、时间触发的天气覆盖）；Configuration Manager（保存/应用完整 UDS 配置、运行时 Apply Sky Configuration）；保存 UDS+UDW 状态用于存档；Sun Lens Flare；时间/天气/室内驱动的后处理组件；室内调整 + 玩家遮挡；水位（焦散/水下雾）；时间/天气控制的环境音效；UDS Onscreen Controls 控件。

**核心内容**：Sky Modifier（数据资产覆盖天空/后处理属性、时间触发）；Configuration Manager（保存/应用配置、运行时 Sky Configuration）；存档序列化（时间/天气状态保存）；Sun Lens Flare；Post Process Component（时间/天气/室内驱动）；Interior Adjustments + Player Occlusion（UDS Occlusion Volume/Portal）；Water Level（Caustics/Underwater Fog/Water Body Classes）；Ambient Sound（时间/天气控制）；Onscreen Controls Widget。

---

## uds-performance-mobile-troubleshooting

**适用于**：性能调优、移动端/主机配置、UDS 安全更新、子蓝图修改、常见运行时问题解决。

**核心内容**：Sky Mode 性能影响；Volumetric Cloud Rendering Mode 与 Sample Scales；Two Layers 性能对比；Sky Light Mode 权衡；Volumetric Fog 可扩展性；Half Rate Tick；Sky Mode Scalability Map；Mobile Category + Platform Feature Levels Map；Fab Launcher 安全更新；Static Properties；Hard Reset Cache / Max Property Cache Period；常见问题修复（黑屏/环境光不更新/Lumen GI 适应慢/云与网格交叉/云模糊拖尾/雾硬线/天空闪烁/低质量体积雾/太空层丢失）。
