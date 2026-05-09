# LayeredBlendPerBody

`LayeredBlendPerBody` 是一套面向 Unreal Engine 动画蓝图的“按身体部位分层混合”插件。它提供了：

- 运行时动画节点 `Layered blend per body part`
- 可持久化的配置资产 `ULbbDefinition`
- 专用图形化编辑器，用于编辑 `Cache Graph` 和多个 `BodyPart Graph`

这套系统的定位不是再做一套通用 ABP，也不是运行时解释执行图，而是：

- 用图形化编辑体验组织姿势处理流程
- 在编辑期把图编译成运行时 `Operators`
- 运行时继续执行轻量、稳定的顺序化程序

## 1. 内容介绍

### 1.1 解决的问题

传统按身体部位混合的实现，通常会把作者直接暴露给一串线性的 `Operators`。这样运行时很直接，但作者侧会有几个明显问题：

- 很难直观看出“先做什么、后做什么”
- 中间 Pose、缓存 Pose、结果 Pose 的关系不清晰
- Slot、Blend、MaskedBlend、Additive 的插入顺序不容易维护
- 不同 BodyPart 之间缺少统一的公共预处理入口

`LayeredBlendPerBody` 的当前方案是：

- 提供一个全局唯一的 `Cache Graph`
- 为每个 `BodyPart` 提供一个独立的 `BodyPart Graph`
- 图只用于作者编辑与编译，不直接作为运行时 source of truth
- 资产中真正用于运行时的是 `CacheProgram` 和 `BodyParts`

### 1.2 核心概念

#### `Cache Graph`

- 全局唯一
- 每帧先执行一次
- 用于生产可复用的 `CachePose`
- 典型用途是公共预处理、制作 Additive、提前缓存某些姿势结果

#### `BodyPart Graph`

- 每个 BodyPart 一张图
- 执行顺序与 `BodyParts` 数组顺序一致
- 输入的 `CurrentPose` 代表进入当前 BodyPart 之前的累计结果
- 图的最终结果会覆盖当前输出，继续传给下一个 BodyPart

#### `Input`

统一输入节点，作者不再面对一组固定输入节点，而是通过属性选择来源：

- `Motion`
- `BasePose`
- `OverlayPose`
- `CurrentPose`
- `CachePose`

其中：

- `Cache Graph` 中只允许读取 `Motion / BasePose / OverlayPose`
- `BodyPart Graph` 中允许读取 `CurrentPose / Motion / BasePose / OverlayPose / CachePose`

#### `CachePose`

- 对作者来说，缓存统一视为 `Pose`
- 系统内部不会要求作者区分“普通 Pose”和“Additive Pose”
- `Blend / MaskedBlend / ApplyAdditive` 都只接收 `Pose`

### 1.3 当前支持的图节点

#### 所有图都可用

- `Input`
- `Blend`
- `Masked Blend`
- `Apply Additive`

#### 仅 `Cache Graph` 可用

- `Save Pose`
- `Make Additive`

#### 仅 `BodyPart Graph` 可用

- `Result`
- `Apply Slot`

### 1.4 运行时结构

当前资产最终会编译出两类运行时数据：

- `CacheProgram`
  - 全局缓存程序
  - 包含 `NamedCacheNames` 和编译后的 `Operators`
- `BodyParts`
  - 每个 BodyPart 一个顺序执行程序
  - 每个程序内部仍然是编译后的 `Operators`

这意味着：

- 图形化结构只存在于编辑器与资产的 `EditorModel`
- 运行时不直接解释 `UEdGraph`
- 运行时仍然是顺序化程序执行

### 1.5 代码结构概览

- `Source/LayeredBlendPerBody`
  - Runtime 模块
  - 定义资产、运行时 operator、执行上下文、AnimNode
- `Source/LayeredBlendPerBodyEditor`
  - Editor 模块
  - 定义资产编辑器、Graph 节点、Graph 编译器、Graph Schema
- `Source/LayeredBlendPerBodyUncooked`
  - UncookedOnly 模块
  - 提供 AnimGraph 节点包装

## 2. 使用说明

### 2.1 创建资产

在内容浏览器中创建一个 `Data Asset`，资产类选择：

- `Lbb Layered Blend Body Definition`

对应类：

- `ULbbDefinition`

资产打开后会进入专用编辑器，而不是默认 Data Asset 详情面板。

### 2.2 编辑器界面

当前编辑器主要包含几个区域：

- `Body Parts`
  - 左侧列表
  - 用于切换 `Cache Graph` 和各个 `BodyPart Graph`
- `Graph`
  - 中间图编辑区
  - 右键可添加当前图允许的节点
- `Details`
  - 右侧属性面板
  - 用于编辑节点参数或 BodyPart 设置
- `Compiled Operators Preview`
  - 查看当前图编译后的运行时 operator 结果
- `Compile Messages`
  - 查看编译错误、警告、性能提示

### 2.3 基本编辑流程

推荐按下面顺序使用：

1. 先打开 `Cache Graph`
2. 根据需要添加 `Input / Blend / Masked Blend / Make Additive / Apply Additive / Save Pose`
3. 用 `Save Pose` 产出供后续 BodyPart 使用的命名缓存
4. 新增一个或多个 `BodyPart`
5. 在各自的 `BodyPart Graph` 中通过 `Input(CachePose)` 读取公共缓存
6. 用 `Apply Slot / Blend / Masked Blend / Apply Additive` 完成每个 BodyPart 的姿势处理
7. 确保 `BodyPart Graph` 的结果连到 `Result`
8. 点击工具栏 `Compile`
9. 确认 `Compile Messages` 没有错误后再保存资产

### 2.4 BodyPart 管理

编辑器支持以下 BodyPart 操作：

- 新增 BodyPart
- 复制 BodyPart
- 删除 BodyPart
- 上移 / 下移 BodyPart

当前快捷键：

- `Insert`
  - 新增 BodyPart
- `Ctrl + D`
  - 复制选中的 BodyPart
- `Delete`
  - 删除选中的 BodyPart
- `Ctrl + Shift + Up`
  - 上移
- `Ctrl + Shift + Down`
  - 下移

注意：

- `BodyPart` 的顺序就是运行时执行顺序
- 越靠后的 BodyPart，看到的 `CurrentPose` 越晚

### 2.5 图连接规则

当前 Graph Schema 的约束比较简单明确：

- 只允许 `Pose` 类型 pin 互连
- 输入 pin 只允许一条连接
- 不允许在同一节点内部自连
- 不允许制造环

因此这套图是标准的有向无环图，编译器会按依赖顺序生成 operator 列表。

### 2.6 编译与保存

资产编辑器会维护一份作者侧的 `EditorModel`，并将其编译为：

- `CacheProgram`
- `BodyParts[].Operators`

保存前会执行编译校验。当前策略是：

- 有错误时阻止保存
- 警告不会阻止保存，但应该认真处理

典型错误包括：

- `Result` 未连接
- `Save Pose` 未填写 `CachePoseName`
- 输入 pin 未连接
- 使用了当前图不允许的节点或输入源
- 引用了不存在的 `CachePose`

### 2.7 在动画蓝图中使用

在 AnimGraph 中添加节点：

- `Layered blend per body part`

对应运行时节点：

- `FLbbAnimNode_LayeredBodyPartBlend`

它暴露了三路姿势输入：

- `Motion`
- `BasePose`
- `OverlayPose`

以及一个定义资产：

- `BodyDefinition`

推荐理解方式：

- `Motion`
  - 主运动姿势
- `BasePose`
  - 供某些混合逻辑使用的基础姿势
- `OverlayPose`
  - 供叠加或上层逻辑使用的姿势

如果某个图没有实际读取 `BasePose` 或 `OverlayPose`，运行时不会额外评估对应输入。

### 2.8 使用建议

- 把公共计算尽量前移到 `Cache Graph`
- 把 `BodyPart Graph` 保持为“消费公共缓存并落到当前输出”的逻辑
- 不要在 `Cache Graph` 里堆叠过多无意义中间节点
- `CachePoseName` 尽量稳定、清晰、可复用
- 同类逻辑优先通过复制 BodyPart 再微调，而不是从零搭图

## 3. 扩展手册

这一节面向开发者，说明如何在当前架构上扩展。

### 3.1 扩展原则

当前架构已经收过边界，扩展时请保持以下原则：

- 不把系统扩展成另一套通用 ABP
- 作者侧继续围绕 `Pose` 流而不是通用值流
- 图只负责作者体验与编译，不负责运行时解释
- 新能力优先编译成现有 runtime operator 体系

如果一个需求已经明显需要：

- 通用变量流
- 条件分支
- 循环
- 状态机
- 任意类型值系统

那通常应该放到 ABP 或别的图系统里，而不是继续扩这套插件。

### 3.2 新增一个图节点

新增节点通常要改三层：

1. Runtime 作者数据层
2. Editor 图节点层
3. Graph 编译层

#### 第一步：定义 NodeData

在：

- `Source/LayeredBlendPerBody/Public/LbbGraphNodeData.h`

新增一个 `FLbbGraphNodeData_*` 结构，只保存作者真正需要编辑的参数，不要把运行时内部数据塞进来。

已有做法可以参考：

- `FLbbGraphNodeData_Blend`
- `FLbbGraphNodeData_SavePose`
- `FLbbGraphNodeData_ApplyAdditive`

#### 第二步：新增 Editor 节点类

在目录：

- `Source/LayeredBlendPerBodyEditor/Public/GraphNodes`
- `Source/LayeredBlendPerBodyEditor/Private/GraphNodes`

新增一对文件：

- `LbbEdGraphNode_YourNode.h`
- `LbbEdGraphNode_YourNode.cpp`

最少要实现这些内容：

- `GetNodeDataStruct()`
- `BuildGraphNodeDescriptor()`
- `AllocatePosePins()`
- `GetNodeTitleText()`
- `ImportNodeData()`
- `ExportNodeData()`

如果节点是作者节点，还需要实现：

- `static bool CompileNode(...)`

#### 第三步：声明节点描述符

`BuildGraphNodeDescriptor()` 需要明确几件事：

- 对应的 `NodeDataStruct`
- 对应的 `NodeClass`
- 显示名
- 输入 pin 列表
- 输出 pin 列表
- 是否是 `Input / Result / SavePose` 之类的特殊节点
- 允许出现在 `Cache` 还是 `BodyPart`
- 是否提供 `CompileNode`

当前 descriptor 是节点自带的，不再集中写死在 registry 里。

#### 第四步：自动注册

当前节点注册机制已经改成反射自动注册。

也就是说：

- 只要你的节点类继承自 `ULbbEdGraphNode`
- 是原生类
- 不是抽象类 / 废弃类
- `BuildGraphNodeDescriptor()` 返回有效 descriptor

它就会在模块启动时自动进入 registry。

不需要再手工去模块里加：

- `RegisterGraphNode()`
- 节点白名单

### 3.3 新增一个运行时 Operator

如果现有节点编译不到已有 operator，就需要新增运行时 operator。

#### 第一步：定义作者态 operator 结构

在：

- `Source/LayeredBlendPerBody/Public/LbbOperators.h`

新增一个 `FLbbPartOperator_*` 结构。

这个结构负责持久化编译结果，常见字段包括：

- 输入 `PoseSource`
- 输出 `PoseTarget`
- 节点参数

#### 第二步：实现 compiled operator

在：

- `Source/LayeredBlendPerBody/Private/LbbOperators.cpp`

新增对应的 `FLbbCompiledOperator` 实现，并补齐：

- `AccumulateRequirements()`
- `Execute()`

如果需要曲线驱动权重，也要接到 `UpdateBlendWeight()` 流程。

#### 第三步：实现 `CreateCompiledOperator()`

在作者态 operator 上实现：

- `CreateCompiledOperator(ILbbOperatorCompileContext& CompileContext)`

这里负责把：

- `FLbbLayeredBodyPartPoseSource`
- `FLbbLayeredBodyPartPoseTarget`

编译为：

- `FLbbCompiledPoseSource`
- `FLbbCompiledPoseTarget`

### 3.4 新增一个输入来源

如果要给 `Input` 新增新的 `PoseSourceType`，通常要同步修改：

- `Source/LayeredBlendPerBody/Public/LbbTypes.h`
- `Source/LayeredBlendPerBody/Private/LbbExecutionContext.cpp`
- `Source/LayeredBlendPerBody/Private/LbbRuntime.cpp`
- `Source/LayeredBlendPerBodyEditor/Private/GraphNodes/LbbEdGraphNode_Input.cpp`
- `Source/LayeredBlendPerBodyEditor/Private/LbbGraphCompiler.cpp`

注意判断这个来源是否：

- 允许在 `Cache Graph` 中读取
- 允许在 `BodyPart Graph` 中读取
- 需要新增运行时输入通道

如果新增来源会把系统推向“通用值图”，应该先停下来重新判断边界。

### 3.5 修改编译规则

图到运行时的主要编译入口在：

- `Source/LayeredBlendPerBodyEditor/Private/LbbGraphCompiler.cpp`

这里负责：

- 校验图结构
- 建立节点 descriptor 上下文
- 解析输入连接
- 生成拓扑顺序
- 分配内部缓存 slot
- 把图节点编译为 `Operators`

如果要改编译规则，优先考虑：

- 是否只影响作者体验
- 是否会影响现有运行时结构
- 是否会增加新的缓存 slot 或新的运行时输入要求

### 3.6 调试建议

扩展这套系统时，建议按下面顺序排查问题：

1. 先看 Graph Editor 中的 `Compile Messages`
2. 再看 `Compiled Operators Preview`
3. 再检查资产中的 `CacheProgram` / `BodyParts`
4. 最后再看运行时 `Execute()` 逻辑

如果一个节点在图里看起来没问题，但运行时不对，通常是下面几类问题：

- `ImportNodeData / ExportNodeData` 不对称
- `BuildGraphNodeDescriptor()` 的 pin 声明和 `AllocatePosePins()` 不一致
- `CompileNode()` 解析错了输入 pin
- `CreateCompiledOperator()` 编译 source / target 时写错类型

## 4. 相关文档

更细的实现文档见：

- [Doc/Lbb_CacheGraph_ImplementationOrder.md](Doc/Lbb_CacheGraph_ImplementationOrder.md)
- [Doc/UNwLayeredBlendBodyDefinition_GraphEditor_Spec.md](Doc/UNwLayeredBlendBodyDefinition_GraphEditor_Spec.md)
- [Doc/FNwAnimNode_LayeredBodyPartBlend_Optimization.md](Doc/FNwAnimNode_LayeredBodyPartBlend_Optimization.md)

说明：

- `Doc/` 下部分文档仍保留历史命名和历史推演内容
- 当前代码应以 `Lbb*` 命名实现和本 `README` 为准
