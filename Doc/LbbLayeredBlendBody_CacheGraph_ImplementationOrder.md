# LbbLayeredBlendBody Cache Graph 实施顺序

- 文档状态: Draft v1
- 更新时间: 2026-05-07
- 插件路径: `Plugins/LayeredBlendPerBody`
- 目标: 在不把系统扩展成另一套 ABP 的前提下，引入公共 `Cache Graph`、统一 `Input` 节点、`SavePose`、`MakeAdditive`、`ApplyAdditive`
- 明确不做:
  - 旧资产迁移
  - 强类型 `Pose / Additive` 校验
  - `InputPoseDefinitions`
  - 公开的 `CompilerTemporaryPose`

## 1. 已确认边界

- 整个系统仍然是“当前帧姿势加工图”，不是状态机，不是播放图，不是通用 AnimGraph。
- 统一使用 `FLbbLayeredBodyPartPoseSource` / `FLbbLayeredBodyPartPoseTarget` 作为作者侧输入输出描述。
- 作者侧语义改成:
  - `OutputPose` -> `CurrentPose`
  - `TemporaryPose` -> `CachePose`
- `Input` 节点只有一个，用户通过属性选择读什么 source。
- 新增一个全局唯一的 `Cache Graph`，它只做公共缓存预处理。
- 每个 `BodyPart Graph` 继续保留一个固定 `Result` 节点。
- 缓存一律视为 `Pose`，不区分“普通 Pose”和“Additive Pose”。
- `Blend`、`MaskedBlend`、`ApplyAdditive` 都只接受 `Pose`，语义正确性由作者自行维护。
- 后续如果开放 `SavePose`，用户可自行命名缓存。

## 2. 最终代码形态

### 2.1 Graph 层级

- `Cache Graph`
  - 全局唯一
  - 每帧执行一次
  - 输入来源仅限 `Motion`、`BasePose`、`OverlayPose`
  - 允许 `Input`、`Blend`、`MaskedBlend`、`MakeAdditive`、`ApplyAdditive`、`SavePose`
  - 不允许 `Result`
  - 不允许读取 `CurrentPose`
  - 不允许读取 `CachePose`

- `BodyPart Graph`
  - 每个 `BodyPart` 一个
  - 输入来源允许 `CurrentPose`、`Motion`、`BasePose`、`OverlayPose`、`CachePose`
  - 允许 `Input`、`Blend`、`MaskedBlend`、`ApplySlot`、`ApplyAdditive`、`Result`
  - 不允许 `SavePose`
  - 不允许 `MakeAdditive`

### 2.2 作者侧节点集合

- 保留:
  - `Result`
  - `ApplySlot`
  - `Blend`
  - `MaskedBlend`

- 删除或替换:
  - 删除固定输入节点:
    - `CurrentPose`
    - `Motion`
    - `BasePose`
    - `OverlayPose`
  - 删除 `ApplyMotionDelta`
  - 新增统一 `Input`
  - 新增 `SavePose`
  - 新增 `MakeAdditive`
  - 新增 `ApplyAdditive`

### 2.3 运行时层级

- 运行时仍然执行“编译后的 operator 列表”，不直接解释执行 `UEdGraph`。
- `Cache Graph` 编译成一段独立的公共 operator program。
- 每个 `BodyPart Graph` 编译成各自的 operator program。
- 作者侧不引入 `CompilerTemporaryPose`，但编译产物内部仍然可以保留匿名临时 slot 索引。
- 用户命名缓存与编译器匿名临时缓存分离:
  - 用户命名缓存走 `CachePoseName`
  - 编译器中间值走内部匿名索引，不进入用户命名空间

## 3. 实施原则

- 先拆“作者侧枚举语义”和“编译后执行语义”，再做图能力扩展。
- 先让运行时支持公共缓存与统一 source，再改编辑器节点与图编译。
- `Cache Graph` 与 `BodyPart Graph` 的节点可用范围必须由 `GraphSchema` 明确限制，不能只靠约定。
- 每个阶段结束都要能单独编译通过。
- 本轮不兼容旧资产，所以允许直接改 authoring 数据结构，不需要保留旧字段兜底。

## 4. 实施顺序

### 阶段 1. 拆作者侧语义与编译期语义

目标:
- 不再让编译后的 `FCompiledPoseSource / Target` 直接复用作者侧枚举。

原因:
- 公开语义要改成 `CurrentPose / CachePose`
- 编译器内部仍需要匿名临时 slot
- 如果两者继续共用一套枚举，后续公共缓存和中间结果会纠缠在一起

建议改动:
- 在运行时编译层新增内部枚举，例如:
  - `ECompiledPoseSourceKind`
  - `ECompiledPoseTargetKind`
- 内部枚举建议至少包含:
  - `Motion`
  - `BasePose`
  - `OverlayPose`
  - `CurrentPose`
  - `NamedCache`
  - `TemporarySlot`
- `FCompiledPoseSource` / `FCompiledPoseTarget` 改成使用内部枚举与 slot index
- `FLbbLayeredBodyPartPoseSource` / `Target` 继续保留为作者侧数据

涉及文件:
- `Source/LayeredBlendPerBody/Public/LbbLayeredBlendBodyDefinition.h`
- `Source/LayeredBlendPerBody/Private/LbbLayeredBlendBodyDefinition.cpp`
- `Source/LayeredBlendPerBody/Public/LbbLayeredBlendBodyExecutionContext.h`
- `Source/LayeredBlendPerBody/Private/LbbLayeredBlendBodyExecutionContext.cpp`
- `Source/LayeredBlendPerBody/Private/LbbAnimNode_LayeredBodyPartBlend.cpp`

完成标志:
- 运行时编译层已经不依赖作者侧 `PoseSourceType` 原值语义。
- 当前主分支功能保持可编译。

### 阶段 2. 重命名作者侧 Pose Source / Target 语义

目标:
- 把作者可见概念调整到最终命名。

建议改动:
- `ELbbLayeredBodyPartPoseSourceType`
  - `OutputPose` -> `CurrentPose`
  - `TemporaryPose` -> `CachePose`
- `ELbbLayeredBodyPartPoseTargetType`
  - `TemporaryPose` -> `CachePose`
- `FLbbLayeredBodyPartPoseSource`
  - `TemporaryPoseName` -> `CachePoseName`
- `FLbbLayeredBodyPartPoseTarget`
  - `TemporaryPoseName` -> `CachePoseName`
- 所有字符串文案同步更新:
  - `Output Pose` -> `Current Pose`
  - `Temp(...)` -> `Cache(...)`

涉及文件:
- `Source/LayeredBlendPerBody/Public/LbbLayeredBlendBodyTypes.h`
- `Source/LayeredBlendPerBody/Private/LbbLayeredBlendBodyDefinition.cpp`
- `Source/LayeredBlendPerBody/Private/LbbLayeredBlendBodyExecutionContext.cpp`
- 任何格式化 source/target 文案的编辑器文件

完成标志:
- 作者侧数据模型里不再出现 `OutputPose` / `TemporaryPose` 旧命名。

### 阶段 3. 扩展 EditorModel，加入 Cache Graph

目标:
- 让资产能同时持久化公共缓存图和各个 BodyPart 图。

建议改动:
- 在 `LbbLayeredBlendBodyEditorModel.h` 中:
  - 删除固定输入节点数据 struct
  - 新增统一 `FLbbLayeredBlendBodyGraphNodeData_Input`
  - 新增 `FLbbLayeredBlendBodyGraphNodeData_SavePose`
  - 新增 `FLbbLayeredBlendBodyGraphNodeData_MakeAdditive`
  - 新增 `FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive`
  - 删除 `FLbbLayeredBlendBodyGraphNodeData_ApplyMotionDelta`
- 新增一个公共图持久化结构，例如:
  - `FLbbLayeredBlendBodyCacheGraphModel`
- `FLbbLayeredBlendBodyEditorModel` 根结构增加:
  - `CacheGraph`
  - `BodyPartGraphs`
- `Version` 直接升版本，不做旧资产迁移

建议的节点数据结构:
- `Input`
  - `FLbbLayeredBodyPartPoseSource SourcePose`
- `SavePose`
  - `FName CachePoseName`
- `MakeAdditive`
  - `FLbbBlendWeight Weight`
  - `ELbbBoneSpace AdditiveSpace`
- `ApplyAdditive`
  - `FLbbBlendWeight Weight`
  - `ELbbBoneSpace AdditiveSpace`

涉及文件:
- `Source/LayeredBlendPerBody/Public/LbbLayeredBlendBodyEditorModel.h`

完成标志:
- 资产模型已具备表达最终图结构的能力。

### 阶段 4. 建立公共缓存执行基础设施

目标:
- 运行时能够在每帧先执行一次 `Cache Graph`，把结果存入命名缓存表。

建议改动:
- 在运行时新增公共缓存程序数据:
  - 编译结果层:
    - `TArray<FInstancedStruct> CacheProgramOperators`
    - `TArray<FName> NamedCacheSlots`
  - 运行时层:
    - `NamedCache` slot 数组
- `FBodyPartExecutionInputs` 改成可读取命名缓存:
  - `Motion`
  - `BasePose`
  - `OverlayPose`
  - `NamedCaches`
- `FOperatorExecutionContext` 扩展:
  - `ResolveNamedCacheSource`
  - `ResolveNamedCacheTarget`
- 保留内部匿名临时 slot 数组，用于编译器落中间结果

重要约束:
- 用户命名缓存只属于 `Cache Graph`
- `BodyPart Graph` 只读，不写

涉及文件:
- `Source/LayeredBlendPerBody/Public/LbbLayeredBlendBodyExecutionContext.h`
- `Source/LayeredBlendPerBody/Private/LbbLayeredBlendBodyExecutionContext.cpp`
- `Source/LayeredBlendPerBody/Public/LbbAnimNode_LayeredBodyPartBlend.h`
- `Source/LayeredBlendPerBody/Private/LbbAnimNode_LayeredBodyPartBlend.cpp`
- `Source/LayeredBlendPerBody/Public/LbbLayeredBlendBodyDefinition.h`

完成标志:
- 运行时已有独立的公共缓存存储与读取通道。

### 阶段 5. 新增或调整运行时 operator

目标:
- 让新的图节点都能编译到运行时 operator。

建议改动:
- 保留现有:
  - `CopyPose`
  - `ApplySlot`
  - `BlendTwoPoses`
  - `MaskedBlend`
- 新增:
  - `MakeAdditive`
  - `ApplyAdditive`
- `SavePose` 不一定需要单独 operator:
  - 如果目标体系已支持“输出到命名 cache”，则 `CopyPose` 即可承担 `SavePose`
- 删除或废弃:
  - `ApplyMotionDelta` 的作者侧入口
  - `Motion Delta` 专用 operator 编译路径

实现建议:
- `MakeAdditive`
  - 输入两个 `Pose`
  - 输出一个 `Pose`
  - 结果直接视为普通 `Pose` 缓存
- `ApplyAdditive`
  - 输入 `Base Pose`
  - 输入 `Additive Pose`
  - 参数 `AdditiveSpace`
  - 输出一个 `Pose`

涉及文件:
- `Source/LayeredBlendPerBody/Public/LbbLayeredBlendBodyDefinition.h`
- `Source/LayeredBlendPerBody/Private/LbbLayeredBlendBodyDefinition.cpp`

完成标志:
- 新的节点语义都能映射到运行时 operator。

### 阶段 6. 编译 Cache Graph

目标:
- 把公共缓存图编译成独立 operator program。

建议改动:
- 在编辑器编译器中新增 `CompileCacheGraph`
- `Cache Graph` 编译规则:
  - 无 `Result`
  - 至少有一个 `SavePose` 才有意义
  - `SavePose.CachePoseName` 必须非空
  - `SavePose.CachePoseName` 必须唯一
  - `Input` 只能读取 `Motion / BasePose / OverlayPose`
  - 不允许 `Input(CachePose)`
  - 不允许 `Input(CurrentPose)`
- `SavePose` 编译时:
  - 分配命名 cache slot
  - 把输入 source 写入该 slot
- 匿名中间节点继续走内部 temporary slot

建议结果结构:
- `FLbbLayeredBlendBodyCacheProgram`
  - `TArray<FInstancedStruct> Operators`
  - `TArray<FName> NamedCacheNames`

涉及文件:
- `Source/LayeredBlendPerBodyEditor/Private/LbbLayeredBlendBodyGraphCompiler.cpp`
- 如有必要，新增 editor-only 辅助头文件

完成标志:
- `Cache Graph` 已可独立编译。

### 阶段 7. 重构 BodyPart Graph 编译

目标:
- BodyPart 图改成消费统一 `Input` 和公共 `CachePose`。

建议改动:
- 删掉“必须存在四个固定输入节点”的校验
- 新的 `Input` 节点编译规则:
  - `CurrentPose` -> 编译到当前 body part 当前输出 source
  - `Motion / BasePose / OverlayPose` -> 编译到外部输入 source
  - `CachePose` -> 编译到命名 cache source
- 删除 `ApplyMotionDelta` 节点编译分支
- 新增 `ApplyAdditive` 节点编译分支
- `BodyPart Graph` 继续要求:
  - 恰好一个 `Result`
  - 无环
  - 输入 pin 连通性正确

建议默认图:
- 新建 `BodyPart Graph` 时自动生成:
  - `Input(CurrentPose)` -> `Result`

涉及文件:
- `Source/LayeredBlendPerBodyEditor/Private/LbbLayeredBlendBodyGraphCompiler.cpp`

完成标志:
- BodyPart 图不再依赖固定 source 节点。

### 阶段 8. 重构 Editor 节点与注册表

目标:
- 图编辑器只暴露最终节点集合，并按图类型限制可创建节点。

建议改动:
- `LbbLayeredBlendBodyEdGraphNode.h/.cpp`
  - 删除固定输入节点类
  - 新增 `ULbbLayeredBlendBodyEdGraphNode_Input`
  - 新增 `ULbbLayeredBlendBodyEdGraphNode_SavePose`
  - 新增 `ULbbLayeredBlendBodyEdGraphNode_MakeAdditive`
  - 新增 `ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive`
  - 删除 `ULbbLayeredBlendBodyEdGraphNode_ApplyMotionDelta`
- `LbbLayeredBlendBodyGraphNodeRegistry`
  - 新增 descriptor
  - 给每个节点标记允许出现的 graph kind
- `LbbLayeredBlendBodyGraphSchema`
  - 右键菜单按 graph kind 过滤
  - `Input` 的详情面板根据 graph kind 过滤可选 source
- `Result` 仍保留固定节点

涉及文件:
- `Source/LayeredBlendPerBodyEditor/Public/LbbLayeredBlendBodyEdGraphNode.h`
- `Source/LayeredBlendPerBodyEditor/Private/LbbLayeredBlendBodyEdGraphNode.cpp`
- `Source/LayeredBlendPerBodyEditor/Private/LbbLayeredBlendBodyGraphNodeRegistry.h`
- `Source/LayeredBlendPerBodyEditor/Private/LbbLayeredBlendBodyGraphNodeRegistry.cpp`
- `Source/LayeredBlendPerBodyEditor/Public/LbbLayeredBlendBodyGraphSchema.h`
- `Source/LayeredBlendPerBodyEditor/Private/LbbLayeredBlendBodyGraphSchema.cpp`

完成标志:
- 节点菜单与节点可见属性已符合最终设计边界。

### 阶段 9. 扩展编辑器界面，支持 Cache Graph

目标:
- 让资产编辑器可在公共缓存图和各 BodyPart 图之间切换。

建议改动:
- 在 Toolkit 中增加 Cache Graph 入口
- 推荐 UI 方案:
  - 左侧列表顶部固定一个 `Cache Graph`
  - 下方仍是 `BodyPart` 列表
- 切换图时:
  - 同步不同 graph model 到同一套 transient `UEdGraph` 视图
- 详情面板中:
  - `Input` 节点需要能编辑 `SourcePose`
  - `SavePose` 节点需要能编辑 `CachePoseName`
  - `MakeAdditive` / `ApplyAdditive` 需要能编辑 `AdditiveSpace` 和 `Weight`

涉及文件:
- `Source/LayeredBlendPerBodyEditor/Private/LbbLayeredBlendBodyDefinitionEditorToolkit.h`
- `Source/LayeredBlendPerBodyEditor/Private/LbbLayeredBlendBodyDefinitionEditorToolkit.cpp`

完成标志:
- 编辑器中能完整创建并编辑 `Cache Graph` 与 `BodyPart Graph`。

### 阶段 10. 清理旧路径并完成自检

目标:
- 删除旧概念残留，确保系统只保留最终模型。

建议改动:
- 删除固定输入节点旧文案和旧分支
- 删除 `ApplyMotionDelta` 作者侧数据、节点类、注册表、编译分支
- 删除与“固定输入节点唯一性”相关的校验逻辑
- 清理任何依赖 `Motion Delta` 专用预计算的 runtime 代码
- 自检构建:
  - `GASP57Editor Win64 Development`
- 手工测试:
  - 新建资产
  - 新建 Cache Graph
  - `Input(Motion) -> SavePose("CachedMotion")`
  - BodyPart 图中 `Input(CurrentPose)` 与 `Input(CachePose="CachedMotion")` 可同时工作
  - `MakeAdditive + SavePose + ApplyAdditive` 链路可保存并消费

完成标志:
- 编译通过
- 新资产工作流完整可用
- 旧节点不再对新系统可见

## 5. 推荐提交粒度

建议按下面的粒度提交，避免一个超大改动难以回退:

1. `CompiledPoseSource/Target` 内外语义拆分
2. `CurrentPose / CachePose` 作者侧命名重构
3. `EditorModel` 新增 `Cache Graph` 与新节点数据
4. 运行时命名缓存基础设施
5. 新 operator: `MakeAdditive / ApplyAdditive / SavePose` 路径
6. `Cache Graph` 编译器
7. `BodyPart Graph` 编译器切换到统一 `Input`
8. 编辑器节点与节点菜单重构
9. Toolkit 界面接入 `Cache Graph`
10. 清理旧路径与回归验证

## 6. 本轮不做的内容

- 不兼容旧资产
- 不区分 `Pose` 与 `Additive` 缓存类型
- 不增加系统级 `InputPoseDefinitions`
- 不支持 `Cache Graph` 读取 `CachePose`
- 不支持 `BodyPart Graph` 写 `CachePose`
- 不支持状态机、条件分支、播放节点
- 不支持跨帧缓存

## 7. 验收清单

- 资产里存在一个全局 `Cache Graph`
- 新建 `BodyPart Graph` 默认是 `Input(CurrentPose) -> Result`
- `Input` 是唯一 source 节点
- `SavePose` 只能在 `Cache Graph` 创建
- `MakeAdditive` 只能在 `Cache Graph` 创建
- `ApplyAdditive` 在 `Cache Graph` 和 `BodyPart Graph` 都可创建
- `Input(CachePose)` 只能在 `BodyPart Graph` 选择已有命名缓存
- `ApplyMotionDelta` 已从作者侧入口移除
- 新系统完整编译通过

## 8. 先后顺序摘要

最稳妥的落代码顺序如下:

1. 先拆作者侧枚举与编译后执行枚举
2. 再改 `CurrentPose / CachePose` 命名
3. 再扩展 `EditorModel`
4. 再做运行时命名缓存执行基础设施
5. 再加 `Cache Graph` 编译器
6. 再切 `BodyPart Graph` 到统一 `Input`
7. 最后改编辑器 UI 与旧路径清理

这个顺序的核心价值是:
- 先把运行时底层支撑搭好
- 再切图能力
- 最后再暴露编辑器入口

这样中途任一阶段失败，都可以在较小范围内回退和定位问题。

## 9. 本轮已落地的结构重构记录

这次代码迭代先把类型边界和文件边界收紧，目标是让后续继续扩展 `Cache Graph` 和新节点时，不再被一大坨公共类型绑死在同一个实现文件里。

- 公共 operator 体系已拆出单独头文件和实现文件
  - `FCompiledOperator` 已改名为 `FLbbCompiledOperator`
  - `FLbbOperatorRequirements`、`FLbbCompiledPoseSource`、`FLbbCompiledPoseTarget`、`ILbbOperatorCompileContext` 已迁到 `LbbLayeredBlendBodyOperators.h`
  - 运行时派生 operator 继续放在 `LbbLayeredBlendBodyOperators.cpp`，只有真正私有的 helper 留在匿名 namespace
- GraphNode 相关共享类型已拆分
  - Graph authoring 数据独立为 `LbbLayeredBlendBodyGraphNodeData.h`
  - 节点描述与编译上下文独立为 editor 私有头文件 `LbbLayeredBlendBodyGraphNodeTypes.h`
  - 编译消息与编译结果独立为 `LbbLayeredBlendBodyGraphCompiler.h`
- 运行时数据结构已独立
  - `FLbbOperatorProgramRuntimeData`
  - `FLbbLayeredBlendBodyRuntimeData`
  - `CacheProgram / BodyParts` 当前都由 `FLbbOperatorProgramRuntimeData` 承载
- 旧路径已清理
  - 删除了原先塞在 `LbbLayeredBlendBodyDefinition.cpp` 里的 compiled/operator 定义
  - 不再把 public 类型包在外层 namespace 里
  - 清掉了不再使用的临时 helper
- 已完成 UE 5.7 构建验证
  - 结果：通过

## 10. 本轮补充：统一 CacheSlot 语义

这一轮把原先运行时和编译器里的 `NamedCache / TemporarySlot` 两套概念合并成了一套 `CacheSlot`。

- 运行时只保留一份共享 `CacheSlots` 数组
  - `Cache Graph` 和 `BodyPart Graph` 都往这份数组里写
  - public named cache 先进入数组，internal cache 再按需追加
- 编译器内的临时缓存名改为保留前缀 + 图域唯一命名
  - 内部名字自动带系统前缀
  - 用户如果手写了系统前缀，只给 warning
  - 编译器会自动避开和用户命名的冲突
- 运行时不再区分“这是 named cache 还是 temporary slot”
  - 统一按 slot index 读取和写入
  - 上层只关心 `CachePoseName`
