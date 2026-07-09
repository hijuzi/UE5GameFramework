# 游戏框架技能

Unreal 核心游戏框架 —— 从 GameInstance 到 Pawn 的完整层级，以及增强输入、子系统、计时器、GameplayTag 和 GAS 能力系统。

---

## gameplay-framework

**适用于**：创建 GameMode / GameState / PlayerController / Pawn / PlayerState / HUD 类；实现玩家登录、生成、复活、Possess 逻辑；决定状态放在何处（服务器 / 复制 / 每玩家 / 跨关卡）。

**核心内容**：GameInstance → GameMode(Server-Only) → GameState(Replicated) → PlayerController → Pawn → PlayerState 的完整框架图；AGameModeBase vs AGameMode（有无 MatchState 状态机）；Possess/OnPossess 生命周期（Possess 是 final，覆写 OnPossess）；默认类配置方式（DefaultPawnClass 等）；GetAuthGameMode vs GetGameState 在 Server/Client 上的区别。

---

## actors-and-components

**适用于**：构建 Actor 和 Component 构成的可重用游戏对象；理解 Actor 生命周期（BeginPlay/Tick/EndPlay）；创建自定义 Component；Actor 间的父子关系和 Owner 概念。

**核心内容**：AActor 完整生命周期（PostInitializeComponents → BeginPlay → Tick → EndPlay → Destroyed）；UActorComponent vs USceneComponent 层级；CreateDefaultSubobject 模式；RootComponent 约定；组件 Tick 管理；Actor 通道的复制规则。

---

## character-and-movement

**适用于**：实现玩家/AI 角色 —— ACharacter 与 UCharacterMovementComponent；配置移动参数（行走/奔跑/蹲伏/飞行/游泳）；处理移动输入。

**核心内容**：ACharacter（CapsuleComponent + SkeletalMesh + MovementComponent 预设）；UCharacterMovementComponent 配置项（MaxWalkSpeed / JumpZVelocity / AirControl / GravityScale）；移动模式（Walking/Falling/Swimming/Flying）；网络移动预测与修正；NavMovement 接口的 AI 适配。

---

## enhanced-input

**适用于**：实现 UE Enhanced Input 系统 —— UInputAction 数据资产、UInputMappingContext 映射上下文、InputModifier 和 InputTrigger；处理玩家输入无论是键盘/手柄/触屏。

**核心内容**：InputAction（数据资产，分离"做什么"和"怎么触发"）；InputMappingContext（将 Action 绑定到物理按键，支持优先级叠加）；Modifier（Swizzle/Axis/Negate/DeadZone）；Trigger（Pressed/Held/Tap/Pulse/Chord）；EnhancedInputComponent 绑定回调；Player Mappable Key 用于自定义键位。

---

## subsystems

**适用于**：实现引擎管理的单例服务 —— UEngineSubsystem（进程级）、UGameInstanceSubsystem（游戏会话级）、UWorldSubsystem（关卡级）、UTickableWorldSubsystem（可 Tick）、ULocalPlayerSubsystem（本地玩家级）。

**核心内容**：五种 Subsystem 的生命周期范围；Initialize/Deinitialize 回调；ShouldCreateSubsystem 条件创建；InitializeDependency 初始化顺序；Blueprint/Python 暴露；Subsystem vs Manager Actor vs GameInstance 覆写的选择决策。

---

## timers-and-async

**适用于**：安排和延迟工作 —— FTimerManager（SetTimer with FTimerHandle）；延时执行；循环计时器；与异步操作的配合；GameThread vs AsyncTask；线程安全的 Lambda 使用。

**核心内容**：GetWorldTimerManager() → SetTimer/ClearTimer/PauseTimer；FTimerHandle 管理；一次性和循环 Timer；Timer 委托（Lambda / UFUNCTION / 原始函数）；AsyncTask 和 ParallelFor；FGraphEventRef 任务图。

---

## gameplay-tags

**适用于**：在 C++ 和蓝图中使用 Gameplay Tags —— 层级化的 FName 标签（FGameplayTag / FGameplayTagContainer）；声明项目中使用的标签；查询标签匹配（Exact/HasAny/HasAll）；与 GAS 的深度集成。

**核心内容**：FGameplayTag（单标签）和 FGameplayTagContainer（标签集合）；RequestGameplayTag 运行时获取；MatchesTag/HasAny/HasAll 查询；标签层级（Parent.Child 隐式匹配）；GameplayTagManager 注册表；INI 声明 vs DataTable 声明。

---

## gameplay-ability-system (GAS)

**适用于**：构建技能、属性和效果 —— UAbilitySystemComponent（ASC）、UGameplayAbility（激活技能/提交消耗/结束技能）、UAttributeSet（属性集，FGameplayAttributeData）、UGameplayEffect（即时/持续/无限 GE）、UAbilityTask（异步步骤）、GameplayCue（网络化表现效果）。

**核心内容**：ASC 放置决策（PlayerState vs Pawn）；IAbilitySystemInterface；属性集的 FGameplayAttributeData + ATTRIBUTE_ACCESSORS 宏；GE 的持续策略（Instant/HasDuration/Infinite）；技能实例化策略（InstancedPerActor/InstancedPerExecution）；网络执行策略（LocalPredicted/ServerOnly）；复制模式（Full/Mixed/Minimal）；Ability Task（WaitDelay/PlayMontageAndWait/WaitGameplayEvent）；GameplayCue 的表现层路由（Static/Actor）；InitGlobalData 初始化要求。
