# 系统功能技能

AI 导航、网络复制、物理碰撞和存档加载等核心系统级功能。

---

## ai-and-navigation

**适用于**：构建 AI —— AIController 驱动的 Pawn、行为树（Behaviour Tree）和黑板（Blackboard）；配置导航网格（NavMesh）；AI 感知系统（AIPerception）；EQS 环境查询。

**核心内容**：AIController → BrainComponent → BehaviorTree；Blackboard Key（变量存储）；Behaviour Tree 节点（Selector/Sequence/Decorator/Service/Task）；NavMesh 烘焙与动态修改；AI MoveTo 和路径跟随；AIPerception（视觉/听觉/伤害感知）；EQS 查询场景最佳点。

---

## networking-and-replication

**适用于**：实现服务器权威的多人游戏 —— 网络角色（Authority/SimulatedProxy/AutonomousProxy）；属性复制（Replicated/ReplicatedUsing）；RPC（Server/Client/NetMulticast）；网络相关的 Actor 所有权和相关性。

**核心内容**：网络角色（ROLE_Authority / ROLE_AutonomousProxy / ROLE_SimulatedProxy）；UPROPERTY(Replicated) 和 OnRep 回调；RPC 声明（Server/Client/NetMulticast + Reliable/Unreliable）；GetLifetimeReplicatedProps 条件复制；Actor 所有权与网络相关性；Dormancy 机制；移动复制与预测。

---

## physics-and-chaos

**适用于**：实现碰撞、物理模拟和空间查询 —— Chaos 物理引擎；碰撞通道与碰撞响应；物理约束（PhysicsConstraint）；射线/球体/胶囊/盒体 Trace；Overlap 事件。

**核心内容**：碰撞通道（Object/Engine 通道 vs Custom 通道）；碰撞响应（Ignore/Overlap/Block）；UCollisionComponent 配置；物理模拟（Simulate Physics / Mass / AngularDamping）；PhysicsConstraint（铰链/弹簧/绳索）；物理材质（PhysicalMaterial）；FMath LineTrace / SphereTrace / CapsuleTrace / BoxTrace / Sweep；HitResult 结构。

---

## save-and-load

**适用于**：持久化/恢复游戏数据 —— USaveGame 系统；定义可序列化的存档 UObject；序列化到文件槽位（SaveGameToSlot/LoadGameFromSlot）；平台抽象层。

**核心内容**：USaveGame 基类与 UPROPERTY(SaveGame)；UGameplayStatics::CreateSaveGameObject / SaveGameToSlot / LoadGameFromSlot / DoesSaveGameExist / DeleteGameInSlot；Async 版本（AsyncSaveGameToSlot 等）；SaveGame 内存大小限制；UserIndex 槽位索引；跨平台存储路径。
