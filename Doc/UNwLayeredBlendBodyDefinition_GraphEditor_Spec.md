# UNwLayeredBlendBodyDefinition 图形化编辑器实现规格

- 文档状态: Draft v1
- 更新时间: 2026-04-30
- 目标资产: `UNwLayeredBlendBodyDefinition`
- 关联运行时类型:
  - `FNwLayeredBlendBodyPart`
  - `FNwLayeredBlendBodyPartOperatorBase`
  - `FNwLayeredBlendBodyPartOperator_CopyPose`
  - `FNwLayeredBlendBodyPartOperator_ApplySlot`
  - `FNwLayeredBlendBodyPartOperator_ApplyAdditive`
  - `FNwLayeredBlendBodyPartOperator_BlendTwoPoses`
  - `FNwLayeredBlendBodyPartOperator_MaskedBlend`

## 1. 背景

当前 `UNwLayeredBlendBodyDefinition` 的作者化方式基于 `BodyParts[].Operators` 直接编辑。  
这套运行时结构已经较为干净，但作者化成本偏高，主要问题如下：

- 设计者需要直接理解 `PoseSource`、`PoseTarget`、`TemporaryPose`、`CopyPose` 等运行时内部概念。
- `Operators` 是线性指令流，不适合表达“先做什么，再和什么混，再在哪一步插 Slot”这种图形化思维。
- `TemporaryPose` 和 `CopyPose` 是编译细节，但当前需要人工介入。
- `ApplyAdditive`、`MaskedBlend`、`Slot` 的插入顺序很难直观看懂。

因此需要新增一个图形化编辑器，解决作者化问题，但不改变当前运行时以 `Operators` 执行的总体方向。

## 2. 核心结论

本方案的核心结论如下：

- 图形化编辑器只解决编辑体验，不让运行时直接执行图。
- 运行时继续只执行 `BodyParts + Operators`。
- 图形编辑结果在编辑期编译回 `BodyParts[].Operators`。
- `UEdGraph` 只作为编辑器里的临时可视化层，不作为资产的持久化 source of truth。
- 资产内持久化一份纯结构化 Graph Model，Graph Model 是编辑期 source of truth。
- `ApplyAdditive` 不做泛化，语义固定为“叠加 Motion Delta”。

## 3. 目标与非目标

### 3.1 目标

- 提供 `UNwLayeredBlendBodyDefinition` 的专用资产编辑器。
- 用图的方式编辑每个 `BodyPart` 的混合流程。
- 设计者只需要理解 `Pose` 流，不需要理解 `TemporaryPose` 与 `CopyPose`。
- 在编辑期将图稳定编译为现有 `Operators`。
- 保持现有运行时性能特征，不引入运行时图解释执行。

### 3.2 非目标

- 不把该系统改造成完整 AnimGraph 子图系统。
- 不在第一版支持 float 图、条件分支、循环、状态机。
- 不在第一版支持通用 additive pose 数据类型。
- 不在第一版引入运行时调试预览器或 Persona 深度集成。
- 不在第一版改写 `FNwAnimNode_LayeredBodyPartBlend` 的 Evaluate 主流程。

## 4. 固定语义

### 4.1 BodyPart 语义

- 一个 `BodyPart` 对应一张图。
- `BodyParts` 数组顺序就是运行时执行顺序。
- 一个 `BodyPart` 图的结果，会覆盖当前累计输出，并成为下一个 `BodyPart` 的输入之一。

### 4.2 CurrentPose 语义

- `CurrentPose` 表示“进入当前 `BodyPart` 图之前，节点当前累计出来的总输出”。
- 它是作者侧名称。
- 它在运行时映射为 `OutputPose`。

### 4.3 ApplyAdditive 语义

`ApplyAdditive` 固定语义为：

- 输入: 一个 `Base Pose`
- 参数: `AdditiveSpace`、`Weight`
- 输出: 一个 `Pose`
- 隐式数据源: 节点外层预计算的 `Motion Delta`

其显示名称建议为 `Apply Motion Delta`，但编译目标仍然是 `FNwLayeredBlendBodyPartOperator_ApplyAdditive`。

该节点不暴露第二个 `Additive Pose` 输入 pin。  
原因是它不是“通用 additive 节点”，而是“叠加 Motion Delta 的专用节点”。

## 5. 总体架构

### 5.1 模块职责

| 模块 | 职责 |
| --- | --- |
| `NvwaAnimationRuntime` | 持有运行时资产类型与 Graph Model 持久化结构 |
| `NvwaAnimationEditor` | 资产编辑器、图编辑 UI、图到 Operator 的编译器、校验器 |
| `NvwaAnimationUncooked` | 本方案不承担主要职责，可不放核心逻辑 |

### 5.2 Source of Truth

编辑期 source of truth 为资产里的 Graph Model。  
运行时 `BodyParts[].Operators` 是 Graph Model 的编译产物。

### 5.3 为什么不直接持久化 UEdGraph

不建议直接把 `UEdGraph` 作为资产持久化数据，原因如下：

- `UNwLayeredBlendBodyDefinition` 位于 `NvwaAnimationRuntime`，不应直接依赖 `UnrealEd/GraphEditor` 类型。
- `UEdGraph` 持久化会把编辑器依赖渗入资产结构，模块边界变差。
- 纯结构化 Graph Model 更利于版本迁移、差异比对、批处理与未来导入导出。
- `UEdGraph` 作为 transient view 可以保留所有图形编辑体验，但不污染运行时资产定义。

## 6. 资产数据模型

### 6.1 新增编辑期持久化结构

建议在 `NvwaAnimationRuntime` 中新增一个独立头文件，例如：

- `Source/NvwaAnimationRuntime/Public/NwLayeredBlendBodyEditorModel.h`

建议新增以下数据结构：

```cpp
USTRUCT()
struct FNwLayeredBlendBodyEditorModel
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	TArray<FNwLayeredBlendBodyPartGraphModel> BodyPartGraphs;
};

USTRUCT()
struct FNwLayeredBlendBodyPartGraphModel
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GraphGuid;

	UPROPERTY(EditAnywhere)
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere)
	FLinearColor DebugColor = FLinearColor::Green;

	UPROPERTY()
	TArray<FNwLayeredBlendBodyGraphNodeModel> Nodes;

	UPROPERTY()
	TArray<FNwLayeredBlendBodyGraphLinkModel> Links;

	UPROPERTY()
	FVector2D SavedViewLocation = FVector2D::ZeroVector;

	UPROPERTY()
	float SavedZoomAmount = 1.f;
};

USTRUCT()
struct FNwLayeredBlendBodyGraphNodeModel
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid NodeGuid;

	UPROPERTY()
	ENwLayeredBlendBodyGraphNodeType NodeType;

	UPROPERTY()
	FVector2D NodePosition = FVector2D::ZeroVector;

	UPROPERTY()
	FInstancedStruct NodeData;
};

USTRUCT()
struct FNwLayeredBlendBodyGraphLinkModel
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid FromNodeGuid;

	UPROPERTY()
	FName FromPinName;

	UPROPERTY()
	FGuid ToNodeGuid;

	UPROPERTY()
	FName ToPinName;
};
```

### 6.2 在资产中挂接 EditorModel

建议在 `UNwLayeredBlendBodyDefinition` 中新增：

```cpp
#if WITH_EDITORONLY_DATA
UPROPERTY()
FNwLayeredBlendBodyEditorModel EditorModel;
#endif
```

`BodyParts` 保留现状，作为运行时编译产物继续序列化。

### 6.3 NodeData 设计原则

`NodeData` 不直接复用运行时 Operator 结构。  
原因是运行时 Operator 结构中包含如下作者不应直接接触的字段：

- `Output`
- `PoseSource`
- `TemporaryPoseName`

建议为图节点单独定义编辑期 payload 结构，仅保存节点自身真实参数。

## 7. 图节点类型

### 7.1 固定节点

每个 `BodyPart` 图固定存在以下节点：

| 节点 | 类型 | 输入 pin | 输出 pin | 是否可删除 |
| --- | --- | --- | --- | --- |
| `Current Pose` | Fixed Input | 无 | `Pose` | 否 |
| `Motion` | Fixed Input | 无 | `Pose` | 否 |
| `Base Pose` | Fixed Input | 无 | `Pose` | 否 |
| `Overlay Pose` | Fixed Input | 无 | `Pose` | 否 |
| `Result` | Fixed Output | `Pose` | 无 | 否 |

说明：

- 固定节点可以移动位置，但不能删除、复制或改类型。
- `Result` 必须且只能有一个输入连接。

### 7.2 作者可放置节点

| 显示名 | 输入 pin | 输出 pin | 关键参数 | 编译目标 |
| --- | --- | --- | --- | --- |
| `Apply Slot` | `Input` | `Result` | `SlotName` | `Operator_ApplySlot` |
| `Blend` | `Base`, `Blend` | `Result` | `Weight` | `Operator_BlendTwoPoses` |
| `Masked Blend` | `Base`, `Blend` | `Result` | `BlendSpace`, `BoneFilter`, `Weight` | `Operator_MaskedBlend` |
| `Apply Motion Delta` | `Base` | `Result` | `AdditiveSpace`, `Weight` | `Operator_ApplyAdditive` |

### 7.3 不对设计者暴露的内部概念

以下概念只允许编译器内部生成，不在图中暴露：

- `CopyPose`
- `TemporaryPose`
- `TemporaryPoseName`
- `OutputPose` 作为手工 source/target

## 8. Pin 语义与连接规则

### 8.1 数据类型

第一版只支持一种作者可见数据类型：

- `Pose`

不开放作者可见的 `AdditivePose`、`Float`、`Bool`、`Exec`。

### 8.2 连接规则

- 输出 pin 可以扇出到多个下游节点。
- 每个输入 pin 最多只能连接一个上游输出 pin。
- 只允许 `Pose -> Pose` 连接。
- 不允许自连。
- 不允许环。
- `Result.Input` 必须有且仅有一个连接。

### 8.3 ApplyAdditive 特殊规则

`Apply Motion Delta` 只有一个 `Base` 输入 pin。  
该节点不允许出现第二个输入 pin。  
其 additive 来源固定为运行时预计算的 `Motion Delta`。

## 9. 图语义

### 9.1 图是值流，不是指令流

作者侧图语义定义为“值流图”：

- 节点输出代表一个 `Pose` 值。
- 节点本身不直接暴露写入目标。
- 设计者只表达“由哪些输入计算出哪些结果”。

### 9.2 Result 语义

`Result` 节点输入所连接的最终 `Pose`，就是当前 `BodyPart` 执行后的结果。  
编译时，该结果会写回运行时 `OutputPose`。

### 9.3 BodyPart 间关系

- 前一个 `BodyPart` 的执行结果会成为下一个 `BodyPart` 的 `CurrentPose`。
- `Motion`、`BasePose`、`OverlayPose` 对所有 `BodyPart` 都是共享的外部输入。
- `TemporaryPose` 只在单个 `BodyPart` 内部由编译器使用，不跨 `BodyPart`。

## 10. 编辑器 UI 规格

### 10.1 打开方式

使用 `UAssetDefinitionDefault + FAssetEditorToolkit` 打开 `UNwLayeredBlendBodyDefinition`。

### 10.2 主界面布局


| 区域 | 内容 |
| --- | --- |
| 顶部 | 操作按钮栏 |
| 左栏 | `BodyPart` 列表与顺序管理 |
| 中栏 | 当前 `BodyPart` 的图编辑区 |
| 右栏 | 当前选中对象的 Details 面板 |

### 10.2 顶部 操作按钮栏

- 支持编译保存

### 10.3 左栏 BodyPart 列表

左栏至少支持以下操作：

- 新增 `BodyPart`
- 删除 `BodyPart`
- 复制 `BodyPart`
- 重命名 `BodyPart`
- 拖拽调整顺序
- 选择当前编辑目标

每个 `BodyPart` 行建议显示：

- `PartName`
- `DebugColor`
- 编译状态图标

### 10.4 中栏图编辑区

图编辑区至少支持以下能力：

- 右键菜单创建节点
- 拖线连接 pin
- 删除连接
- 框选、移动节点
- 复制粘贴作者节点
- 自动聚焦当前图
- Undo/Redo

### 10.5 右栏 Details

右栏根据选中对象变化：

- 选中 `BodyPart` 时显示 `PartName`、`DebugColor`
- 选中作者节点时显示节点参数
- 选中空白时显示当前图设置
- 不直接开放 raw `BodyParts[].Operators` 的编辑入口

直接复用默认 Details 面板编辑以下参数：

- `FNwAnimationBlendWeightData`
- `FInputBlendPose`
- `FName SlotName`
- `ENwBoneSpace`



## 11. 编译器规格

### 11.1 编译输入与输出

- 输入: `FNwLayeredBlendBodyEditorModel`
- 输出: `TArray<FNwLayeredBlendBodyPart>`

### 11.2 编译阶段

建议编译管线分为以下阶段：

1. 结构校验
2. 可达性分析
3. DAG 拓扑排序
4. 消费者计数分析
5. 临时寄存分配
6. 图节点降级为运行时 Operators
7. 覆盖写回 `BodyParts`

### 11.3 固定节点到运行时 Source 的映射

| 图节点 | 运行时 Source |
| --- | --- |
| `Current Pose` | `OutputPose` |
| `Motion` | `Motion` |
| `Base Pose` | `BasePose` |
| `Overlay Pose` | `OverlayPose` |

### 11.4 节点降级规则

| 图节点 | 运行时 Operator |
| --- | --- |
| `Apply Slot` | `FNwLayeredBlendBodyPartOperator_ApplySlot` |
| `Blend` | `FNwLayeredBlendBodyPartOperator_BlendTwoPoses` |
| `Masked Blend` | `FNwLayeredBlendBodyPartOperator_MaskedBlend` |
| `Apply Motion Delta` | `FNwLayeredBlendBodyPartOperator_ApplyAdditive` |

### 11.5 CopyPose 生成规则

`CopyPose` 只能由编译器自动生成，生成场景限定如下：

- `Result` 直接连接固定输入节点时，需要用 `CopyPose` 将该 source 写入 `OutputPose`
- 一个中间结果被多个下游节点消费，需要先物化到 `TemporaryPose`
- 某个值必须在后续覆盖 `OutputPose` 之前被保留下来
- 最终结果位于 `TemporaryPose`，需要拷回 `OutputPose`

### 11.6 TemporaryPose 分配规则

- `TemporaryPose` 由编译器自动编号
- 不在 EditorModel 中持久化名字
- 不允许设计者手填 `TemporaryPoseName`
- 可以先使用简单递增分配
- 后续可增加生命周期复用优化

### 11.7 编译优化约束

第一版编译器至少满足以下约束：

- 删除与 `Result` 无关的死节点
- 不为固定输入节点生成无意义 `CopyPose`
- 单消费者直线链路优先原地编译到 `OutputPose`
- 多消费者结果才物化为 `TemporaryPose`

## 12. 从现有 Operators 反向导入图

### 12.1 导入触发条件

当满足以下条件时，编辑器自动执行一次导入：

- `EditorModel.BodyPartGraphs` 为空
- `BodyParts` 非空

### 12.2 导入规则

导入时按当前 `BodyParts` 顺序建立图，并为每个 `BodyPart` 创建一张图。

导入算法要点如下：

1. 创建固定节点: `Current Pose`、`Motion`、`Base Pose`、`Overlay Pose`、`Result`
2. 建立运行时位置映射:
   - `OutputPose -> Current Pose`
   - `TemporaryPose[n] -> 某个图值`
3. 顺序遍历当前 `Operators`
4. 对作者可见 Operator 创建图节点并连线
5. 对 `CopyPose` 只更新“位置映射”，不创建作者节点
6. 最后将当前 `OutputPose` 映射值连接到 `Result`

### 12.3 导入后的行为

- 导入成功后立即触发一次编译
- 资产标记为 dirty
- 此后 Graph Model 成为作者化 source of truth
- Toolkit 内将 raw `BodyParts` 视为只读编译产物，不作为手工编辑入口

## 13. 校验规则

### 13.1 Error

以下情况必须视为编译错误：

- 缺少任意一个固定节点
- `Result.Input` 未连接
- 图中存在环
- 任意输入 pin 缺少必须连接
- `Apply Slot` 的 `SlotName == None`
- `Masked Blend` 的 `BoneFilter` 为空
- `Weight` 设为使用曲线，但 `BlendCurveName == None`
- Link 指向不存在的节点或不存在的 pin

### 13.2 Warning

以下情况建议给出警告但允许保存 EditorModel：

- 存在与 `Result` 无关的死节点
- `BodyPart.PartName` 重复
- `DebugColor` 保持默认值
- 图里存在未使用的固定输入

### 13.3 信息输出

编译结果面板至少显示：

- 当前资产是否成功编译
- 每个 `BodyPart` 的错误数与警告数
- 错误定位到节点或连接

## 14. Save、Undo、事务与一致性

### 14.1 Undo/Redo

以下对象应支持事务：

- `UNwLayeredBlendBodyDefinition`
- Graph Model 结构变更
- 临时 `UEdGraph` 节点与连接

### 14.2 自动编译策略

建议采用“编辑即编译”的策略：

- 节点新增、删除、连线变化后自动编译
- 节点参数变化后自动编译
- `BodyPart` 顺序变化后自动编译

原因如下：

- 图到 Operator 的编译成本很低
- 资产中的 `BodyParts` 始终保持最新
- 不依赖保存时再临时同步

### 14.3 Save 行为

第一版建议：

- Toolkit 的 Save 命令先执行校验与编译
- 若存在 Error，则拒绝保存并在 Compile 面板中提示

注意：

- 若后续发现存在绕过 Toolkit 的保存路径，再补资产级 `PreSave` 保护

## 15. 类与文件建议

以下为建议的类清单，不要求一次性全部实现，但总体命名应保持清晰一致。

### 15.1 Runtime 侧

| 类型 | 建议名称 |
| --- | --- |
| EditorModel 结构头 | `NwLayeredBlendBodyEditorModel.h` |
| Graph 节点枚举 | `ENwLayeredBlendBodyGraphNodeType` |
| Graph 节点 payload 基类 | `FNwLayeredBlendBodyGraphNodeDataBase` |
| Graph 节点 payload | `..._ApplySlot`、`..._Blend`、`..._MaskedBlend`、`..._ApplyAdditive` |

### 15.2 Editor 侧

| 类型 | 建议名称 |
| --- | --- |
| AssetDefinition | `UNwAssetDefinition_LayeredBlendBodyDefinition` |
| Toolkit | `FNwLayeredBlendBodyDefinitionEditorToolkit` |
| Graph ViewModel | `FNwLayeredBlendBodyDefinitionEditorViewModel` |
| Transient UEdGraph | `UNwLayeredBlendBodyEdGraph` |
| Transient EdGraph Node 基类 | `UNwLayeredBlendBodyEdGraphNode` |
| Graph Schema | `UNwLayeredBlendBodyGraphSchema` |
| Compiler | `FNwLayeredBlendBodyGraphCompiler` |
| Validator | `FNwLayeredBlendBodyGraphValidator` |

## 16. 推荐实现顺序

### 16.1 Phase 1

- 建立 EditorModel 持久化结构
- 实现专用 Asset Editor
- 实现 `BodyPart` 列表
- 实现固定节点与四类作者节点
- 实现图到 Operator 的单向编译
- 实现基础校验

### 16.2 Phase 2

- 实现从现有 `Operators` 反向导入图
- 增加编译结果面板
- 增加 `SlotName` 下拉选择体验
- 增加 `BoneFilter` 更友好的编辑体验
- 增加简单自动布局

### 16.3 Phase 3

- 增加只读运行时 Operator 预览
- 增加更细粒度的编译优化
- 增加中间值可视化与调试辅助

## 17. 明确不做的设计

以下设计当前明确不采用：

- 运行时直接解释执行图
- 在图中暴露 `TemporaryPoseName`
- 在图中暴露 `CopyPose`
- 将 `ApplyAdditive` 改造成通用双输入 additive 节点
- 直接持久化 `UEdGraph` 作为资产主数据

## 18. 最终落地标准

当满足以下条件时，认为该编辑器方案达到第一版落地标准：

- 设计者无需手工编辑 `PoseSource / PoseTarget / TemporaryPose`
- 设计者可以通过图直接表达 Slot 插入顺序、Blend 顺序、MaskedBlend 顺序
- 图可以稳定编译为当前运行时 `Operators`
- 运行时 `Evaluate` 主流程无需引入图解释执行
- 编辑器侧可以从已有 `BodyParts` 初始化图
