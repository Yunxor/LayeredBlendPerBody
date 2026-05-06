# FNwAnimNode_LayeredBodyPartBlend 优化记录

- 记录时间: 2026-04-22 17:41:48 +08:00
- 插件路径: `Plugins/NvwaAnimation`
- 目标节点: `FNwAnimNode_LayeredBodyPartBlend`
- 引擎版本: UE 5.7

## 背景

本次调整的目标有两类:

1. 修复 `FNwAnimNode_LayeredBodyPartBlend` 中已经确认的 4 处实现错误。
2. 在不改变节点外部接口和大体行为语义的前提下，完成第二阶段的低风险性能优化。

涉及文件:

- `Source/NvwaAnimationRuntime/Public/NwAnimNode_LayeredBodyPartBlend.h`
- `Source/NvwaAnimationRuntime/Private/NwAnimNode_LayeredBodyPartBlend.cpp`

## 本次修复内容

### 1. 修复骨骼权重缓存模型

旧实现中，`CreateMaskWeights` 得到的是基于 `USkeleton` 的骨骼权重，但运行时直接将这份数据作为当前 `RequiredBones` / `CompactPose` 的权重使用。这会导致:

- 当前评估骨骼不是完整 skeleton 时，权重索引语义不一致
- `BlendPosesPerBoneFilter` 读取到的 per-bone 数据和当前 pose 不匹配
- 结果层面可能混错骨骼
- 性能层面每帧做了不必要的全量工作

本次改动将其拆成两级缓存:

- `SkeletonBoneWeights`
  - 保存基于 skeleton 生成的原始 mask
- `DesiredBoneWeights`
  - 在 `UpdateCachedBoneData` 中根据当前 `RequiredBones` 重建
  - 保证后续 `CurrentBoneWeightsLS/MS` 的来源是当前 compact pose 语义

这样处理后，节点的骨骼权重行为与引擎原生 `FAnimNode_LayeredBoneBlend` 的思路保持一致。

### 2. 修复 Local Space / Mesh Space 权重写反

旧实现中:

- mesh-space 权重被写入 `CurrentBoneWeightsLS`
- local-space 权重被写入 `CurrentBoneWeightsMS`

最终导致:

- Mesh Space 混合阶段拿到的是 Local Space 的权重
- Local Space 混合阶段拿到的是 Mesh Space 的权重

本次修复后:

- `BodyPartWeightsMS -> CurrentBoneWeightsMS`
- `BodyPartWeightsLS -> CurrentBoneWeightsLS`

保证了权重来源和混合阶段一致。

### 3. 修复 SlotEvaluatePose 输出上下文错误

旧实现中，在每个 body part 生成临时 `OutputPose` 时，slot 混合结果被错误写入了最终 `Output`，而不是当前 body part 的临时 pose。

这会导致:

- 带 slot 的部位结果污染全局输出
- 当前 body part 后续 additive / overlay 逻辑基于错误数据继续执行

本次修复后:

- `SlotEvaluatePose` 的输出改为写入当前部位的 `OutputPose`
- 保证每个 body part 的 slot 处理在自己的局部结果上完成

### 4. 修复 Overlay 标量混合错误

旧实现中使用了一段危险逻辑，将单个 `float` 的地址伪装成整段骨骼权重数组传给 `BlendTwoPosesTogetherPerBone`。

这会导致:

- 越界读取内存
- 混合结果不可预测
- 性能分析结果失真

本次修复后:

- 改为使用 `FAnimationRuntime::BlendTwoPosesTogether`
- 按标量 `OverlayBlendWeight` 在 `OutputPose` 和 `Motion` 之间做正确插值

新的语义是:

- 当 overlay 权重为 1 时，保留当前 body part 的 overlay 结果
- 当 overlay 权重不是 1 时，将当前 body part 结果与 `Motion` 做一次标量插值

## 第二阶段低风险性能优化

### 1. 增加运行时 relevance 标记

在 `FRuntimeData` 中增加了以下快速判定状态:

- `bHasRelevantLocalSpaceBlend`
- `bHasRelevantMeshSpaceBlend`
- `bHasRelevantOverlayBlendValue`
- `bNeedsMeshSpaceAdditive`
- `bNeedsLocalSpaceAdditive`

用途:

- `Update` 阶段决定是否真的需要更新 `BasePose`、`OverlayPose`、slot 权重
- `Evaluate` 阶段决定是否可以直接透传 `Motion`
- additive delta 只在确实需要的空间中计算

### 2. Motion 直通 Early-Out

如果本帧满足以下任一情况:

- 没有 body part
- 没有 relevant 的 local/mesh layered blend
- 没有需要逐部位生成的 overlay 结果

则在 `Evaluate` 中直接输出 `Motion`，跳过:

- `BasePose` 评估
- `OverlayPose` 评估
- additive delta 生成
- 每个 body part 的目标 pose 构建
- 两次 `BlendPosesPerBoneFilter`

这个优化可以显著减少“结果等于 Motion，但仍然完整跑一遍节点”的浪费。

### 3. Additive Delta 按需计算

旧实现中:

- 总是计算 Local Space additive delta
- 总是计算 Mesh Space additive delta

新实现中:

- 仅在 `bNeedsLocalSpaceAdditive` 为 true 时才生成 `AdditiveDeltaLS`
- 仅在 `bNeedsMeshSpaceAdditive` 为 true 时才生成 `AdditiveDeltaMS`

这样可以避免每帧多做一到两次全姿势差值转换。

### 4. 减少每帧临时数组构造

旧实现中存在多类每帧临时开销:

- `FBlendWeightsData::UpdateBlendWeights` 内每次构造临时指针数组
- `UpdateBlendWeights` 中重复创建 body-part 权重数组
- `CurrentBoneWeights` 每次先 `Reset + AddZeroed`，随后内部又被清零

本次优化后:

- `UpdateBlendWeights` 改为直接逐成员更新，不再构造临时权重指针数组
- `BodyPartWeightsLS` / `BodyPartWeightsMS` 持久化缓存
- `CurrentBoneWeightsLS` / `CurrentBoneWeightsMS` 只在尺寸变化时重新分配

### 5. 跳过无效 Slot 查询

对于 `SlotNodeName == None` 的 body part:

- 直接把 `SlotNodeWeight / SourceWeight / TotalNodeWeight` 清零
- 跳过 `GetSlotWeight`
- 跳过 `UpdateSlotNodeWeight`

这能减少无意义的 proxy 查询。

### 6. BodyDefinition 重新初始化更安全

`ReinitRuntimeData()` 现在会先调用 `RuntimeData.Reset()`，再重新从定义构建运行时数据。

这样可以避免:

- 切换 `BodyDefinition`
- 清空 `BodyDefinition`
- 或者重新设置定义时

旧缓存残留在运行时结构中。

## 当前节点的 Evaluate 语义

修复后的评估流程可以概括为:

1. 先评估 `Motion`
2. 若本帧不需要 layered body part 处理，则直接输出 `Motion`
3. 评估 `BasePose`
4. 按需生成 `Motion` 相对 `BasePose` 的 additive delta
5. 评估 `OverlayPose`
6. 对每个 body part 构建目标 pose:
   - 可选 slot 混合
   - 可选 additive motion delta
   - 可选与 `Motion` 的 overlay 标量插值
7. 使用 Mesh Space 权重做一次 per-bone filter blend
8. 使用 Local Space 权重做一次 per-bone filter blend
9. 输出最终结果

可以理解为:

- `Motion` 决定默认动态底板
- `OverlayPose` 决定想保留的造型
- `AdditiveDelta` 把 `Motion` 的动态变化灌入 `OverlayPose`
- `BodyDefinition` 决定哪些骨骼在何种空间下接受这份结果

## 编译验证

验证环境:

- 引擎路径: `C:\Data\Epic\UE_5.7`
- 构建目标: `LyraEditor Win64 Development`

使用命令:

```powershell
& 'C:\Data\Epic\UE_5.7\Engine\Build\BatchFiles\Build.bat' LyraEditor Win64 Development 'd:\Projects\UEProject\Lyra57\Lyra57.uproject' -NoHotReloadFromIDE -WaitMutex
```

验证结果:

- 编译通过
- `NvwaAnimationRuntime`
- `NvwaAnimationUncooked`
- `NwAnimNode_LayeredBodyPartBlend.cpp`
- `NwAnimGraphNode_LayeredBodyPartBlend.cpp`

均已在 UE 5.7 下完成本次构建验证

## 后续可选工作

如果后续继续优化，可优先考虑以下方向:

1. 增加 Unreal Insights 级别的更细粒度统计埋点
2. 继续分析 Curve / Attributes 的语义是否需要进一步收敛
3. 评估是否将“按部位手调权重”逐步转向基于 motion transfer 的更自动化方案
4. 若 body part 数量较多，可继续研究配方分组和目标 pose 复用

