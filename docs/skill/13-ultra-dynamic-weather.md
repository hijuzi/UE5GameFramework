# Ultra Dynamic Weather (UDW) 技能

动态天气与环境效果系统 —— 涵盖天气粒子、风力闪电、季节温度、材质特效和空间天气分布。

---

## udw-setup-and-state

**适用于**：设置 UDW（需依赖 UDS）和控制天气状态 —— 添加 UDW 到关卡；Basic Controls（Weather Preset / Wind Direction）；七个天气状态值 + 材质状态；Weather Settings Presets；Change Weather；Manual Weather State（逐值覆盖）；采样天气（Get Cloud Coverage / Get Display Name for Current Weather）；天气事件分发器（Started/Finished Raining、Getting Cloudy、Weather Display Name Changed）；Actor Weather Status 组件。

**核心内容**：UDW Actor 添加（需要 UDS 先行配置）；七个 Weather State 值（Rain/Snow/Dust/Wetness/Fog/Wind/Coverage）；Weather Presets（晴天/雨天/暴雪/沙尘暴等）；Change Weather 运行时切换；Manual Weather State 逐值手动控制；Get Cloud Coverage / Get Display Name 采样；事件分发器（下雨开始/停止、变天、天气名称变化）；Actor Weather Status 组件（每个 Actor 的天气暴露追踪）。

---

## udw-particles-lightning-wind-sounds

**适用于**：配置 UDW 的渲染效果 —— Rain/Snow/Dust 粒子（含 Splash Particles）；粒子碰撞模式（Simple/Distance Field/None/Kill Sphere）；共享粒子设置；Lightning（闪电极闪烁 + 遮蔽闪电、Strikable Actor Interface）；Wind（方向/偏移、Debris 碎片、Gust 阵风、Directional Source 布料/SpeedTree、Physics Force、Camera Shake）；天气音效（风/雨/雷 + Close Thunder Delay Per KM）；UDS Occlusion Portal 声音遮蔽；Environment Sounds（5.1 MetaSounds 格式）。

**核心内容**：Rain/Snow/Dust Particle（Spawn Rate / Velocity / Size / LifeTime）；Splash Particle 碰撞溅射；Particle Collision（Simple/Distance Field/None/Kill Sphere）；Lightning（Flash + Obscured 遮蔽闪电、Strikable Actor Interface 雷击接口）；Wind（Wind Direction + Variation / Debris / Gusts / Directional Source / Physics Force / Camera Shake）；Weather Sound Effects（Wind/Rain/Thunder 分层音量）；UDS Occlusion Portal 室内外声音遮蔽；Environment Sounds（5.1 MetaSounds + ChangeEnvironmentSound）。

---

## udw-material-and-screen-effects

**适用于**：添加天气响应的材质效果和屏幕空间效果 —— Surface Weather Effects（表面湿润/雪/尘/滴落/水滴）；Dynamic Landscape Weather Effects V3（DLWE 地形交互、拖尾/涟漪）；Glass Window Rain Drips（窗户雨滴）；Foliage Wind Movement（植被风吹摇摆）；Water Surface Rain Ripples（水面雨涟漪）；Sample UDW Material State / Season / Wind 材质节点；Rainbow（彩虹）/ Screen Droplets / Screen Frost / Heat Distortion / Post Process Wind Fog；Puddle Fluid Volume（水坑流体交互）；Weather Occlusion Volume（天气遮蔽体积）。

**核心内容**：Surface Weather Effects（Wetness/Snow/Dust/Dripping/Droplets 材质效果）；DLWE V3（Dynamic Landscape Weather Effects、Trails/Ripples 交互组件）；Foliage Wind Movement（植被风力摇摆参数）；Sample UDW Material State / Season / Wind 材质蓝图节点；Rainbow / Screen Droplets / Screen Frost / Heat Distortion / PP Wind Fog 屏幕效果；Puddle Fluid Volume（流体交互水坑）；Dripping Mesh Particles（滴水粒子）；Freezing Breath（寒冷呼吸雾）；Rain Drip Spline（屋檐滴水 + 冰柱）；Weather Occlusion Volume（区域内排除天气效果）。

---

## udw-random-seasons-temperature

**适用于**：配置动态天气变化、季节、气候和温度 —— Random Weather Variation（随机间隔/每日/每小时触发、天气类型概率、Transition Length）；Seasons（0-4 浮点数、Set Season、Season Mode 基于日期驱动、Meteorological vs Astronomical）；Climate Presets（真实世界气象数据概率 + 温度范围）；Temperature（Get Current Temperature 华氏/摄氏、Temperature Bias、每季节 Min/Max、全局 vs 局部采样、Interior Temperature）；Temperature Volumes；UDW Thermometer Widget。

**核心内容**：Random Weather Variation（Random Interval/Daily/Hourly 随机触发模式）；Weather Type Probabilities 概率配置；Transition Length 过渡时长；Seasons（0-4 Float / Set Season / Season Mode 日期驱动 / Meteorological vs Astronomical）；Climate Presets（基于真实气候的概率和温度范围）；Get Current Temperature（摄氏/华氏）、Temperature Bias、Min/Max per Season；Temperature Volume（局部温度区域）；UDW Thermometer Widget 温度计 UI。

---

## udw-spatial-weather

**适用于**：在关卡特定区域应用天气 —— Weather Override Volumes（样条定义任意形状、Transition Width、Priority、Apply Wind Direction、Climate Preset、运行时 ChangeWeather/ChangeToRandomWeatherVariation）；Radial Storms（圆形风暴 Actor、外部可见的远处风暴带云/雾/遮蔽闪电、生成系统、Fade In/Out、Move Over Time）；Weather Above Volumetric Clouds 调整（云层之上天气设置）；Weather Mask 系统（Brush / Projection Box / Brush Painter Editor Utility 编辑器笔刷遮罩）；Control Point Location Source。

**核心内容**：Weather Override Volume（Spline 定义形状、Transition Width 过渡宽度、Priority 优先级、Apply Wind Direction、运行时 ChangeWeather / ChangeToRandomWeatherVariation）；Radial Storm（圆形风暴、外部可见云/雾/闪电、Fade In/Out、Move Over Time 移动风暴）；Weather Above Volumetric Clouds（云层上方的不同天气）；Weather Mask（Brush 笔刷 / Projection Box 投影盒 / Brush Painter Editor Utility 编辑器绘制）；Control Point Location Source。
