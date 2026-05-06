// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbLayeredBlendBodyGraphCompiler.h"
#include "LbbLayeredBlendBodyDefinition.h"

namespace LbbLayeredBlendBodyPartEditor
{
	namespace
	{
		const FName PinPose(TEXT("Pose"));
		const FName PinInput(TEXT("Input"));
		const FName PinBase(TEXT("Base"));
		const FName PinBlend(TEXT("Blend"));
		const FName PinResult(TEXT("Result"));

		struct FPinTargetKey
		{
			FGuid NodeGuid;
			FName PinName = NAME_None;

			bool operator==(const FPinTargetKey& Other) const
			{
				return NodeGuid == Other.NodeGuid && PinName == Other.PinName;
			}
		};

		uint32 GetTypeHash(const FPinTargetKey& Key)
		{
			return HashCombine(GetTypeHash(Key.NodeGuid), GetTypeHash(Key.PinName));
		}

		struct FGraphContext
		{
			const FLbbLayeredBlendBodyPartGraphModel& GraphModel;
			TMap<FGuid, const FLbbLayeredBlendBodyGraphNodeModel*> NodesByGuid;
			TMap<FPinTargetKey, const FLbbLayeredBlendBodyGraphLinkModel*> InputLinks;
			const FLbbLayeredBlendBodyGraphNodeModel* CurrentPoseNode = nullptr;
			const FLbbLayeredBlendBodyGraphNodeModel* MotionNode = nullptr;
			const FLbbLayeredBlendBodyGraphNodeModel* BasePoseNode = nullptr;
			const FLbbLayeredBlendBodyGraphNodeModel* OverlayPoseNode = nullptr;
			const FLbbLayeredBlendBodyGraphNodeModel* ResultNode = nullptr;

			explicit FGraphContext(const FLbbLayeredBlendBodyPartGraphModel& InGraphModel)
				: GraphModel(InGraphModel)
			{
			}
		};

		static bool HasErrors(const TArray<FCompileMessage>& Messages)
		{
			for (const FCompileMessage& Message : Messages)
			{
				if (Message.Severity == EMessageSeverity::Error)
				{
					return true;
				}
			}

			return false;
		}

		static void AddMessage(
			TArray<FCompileMessage>& OutMessages,
			const EMessageSeverity::Type Severity,
			const int32 BodyPartIndex,
			const FGuid& NodeGuid,
			const FString& Message)
		{
			FCompileMessage& NewMessage = OutMessages.AddDefaulted_GetRef();
			NewMessage.Severity = Severity;
			NewMessage.BodyPartIndex = BodyPartIndex;
			NewMessage.NodeGuid = NodeGuid;
			NewMessage.Message = Message;
		}

		static bool IsFixedNodeType(const ELbbLayeredBlendBodyGraphNodeType NodeType)
		{
			return NodeType == ELbbLayeredBlendBodyGraphNodeType::CurrentPose
				|| NodeType == ELbbLayeredBlendBodyGraphNodeType::Motion
				|| NodeType == ELbbLayeredBlendBodyGraphNodeType::BasePose
				|| NodeType == ELbbLayeredBlendBodyGraphNodeType::OverlayPose;
		}

		static bool IsAuthorNodeType(const ELbbLayeredBlendBodyGraphNodeType NodeType)
		{
			return NodeType == ELbbLayeredBlendBodyGraphNodeType::ApplySlot
				|| NodeType == ELbbLayeredBlendBodyGraphNodeType::Blend
				|| NodeType == ELbbLayeredBlendBodyGraphNodeType::MaskedBlend
				|| NodeType == ELbbLayeredBlendBodyGraphNodeType::ApplyMotionDelta;
		}

		static void GetInputPinNames(const ELbbLayeredBlendBodyGraphNodeType NodeType, TArray<FName, TInlineAllocator<2>>& OutPinNames)
		{
			OutPinNames.Reset();

			switch (NodeType)
			{
			case ELbbLayeredBlendBodyGraphNodeType::Result:
				OutPinNames.Add(PinPose);
				break;
			case ELbbLayeredBlendBodyGraphNodeType::ApplySlot:
				OutPinNames.Add(PinInput);
				break;
			case ELbbLayeredBlendBodyGraphNodeType::Blend:
			case ELbbLayeredBlendBodyGraphNodeType::MaskedBlend:
				OutPinNames.Add(PinBase);
				OutPinNames.Add(PinBlend);
				break;
			case ELbbLayeredBlendBodyGraphNodeType::ApplyMotionDelta:
				OutPinNames.Add(PinBase);
				break;
			default:
				break;
			}
		}

		static void GetOutputPinNames(const ELbbLayeredBlendBodyGraphNodeType NodeType, TArray<FName, TInlineAllocator<1>>& OutPinNames)
		{
			OutPinNames.Reset();

			switch (NodeType)
			{
			case ELbbLayeredBlendBodyGraphNodeType::CurrentPose:
			case ELbbLayeredBlendBodyGraphNodeType::Motion:
			case ELbbLayeredBlendBodyGraphNodeType::BasePose:
			case ELbbLayeredBlendBodyGraphNodeType::OverlayPose:
				OutPinNames.Add(PinPose);
				break;
			case ELbbLayeredBlendBodyGraphNodeType::ApplySlot:
			case ELbbLayeredBlendBodyGraphNodeType::Blend:
			case ELbbLayeredBlendBodyGraphNodeType::MaskedBlend:
			case ELbbLayeredBlendBodyGraphNodeType::ApplyMotionDelta:
				OutPinNames.Add(PinResult);
				break;
			default:
				break;
			}
		}

		static bool IsValidInputPin(const ELbbLayeredBlendBodyGraphNodeType NodeType, const FName PinName)
		{
			TArray<FName, TInlineAllocator<2>> InputPins;
			GetInputPinNames(NodeType, InputPins);
			return InputPins.Contains(PinName);
		}

		static bool IsValidOutputPin(const ELbbLayeredBlendBodyGraphNodeType NodeType, const FName PinName)
		{
			TArray<FName, TInlineAllocator<1>> OutputPins;
			GetOutputPinNames(NodeType, OutputPins);
			return OutputPins.Contains(PinName);
		}

		static const FLbbLayeredBlendBodyGraphLinkModel* FindInputLink(
			const FGraphContext& GraphContext,
			const FGuid& NodeGuid,
			const FName& PinName)
		{
			return GraphContext.InputLinks.FindRef(FPinTargetKey{NodeGuid, PinName});
		}

		static FGuid AddNode(
			FLbbLayeredBlendBodyPartGraphModel& GraphModel,
			const ELbbLayeredBlendBodyGraphNodeType NodeType,
			const FVector2D& Position)
		{
			FLbbLayeredBlendBodyGraphNodeModel& NodeModel = GraphModel.Nodes.AddDefaulted_GetRef();
			NodeModel.NodeGuid = FGuid::NewGuid();
			NodeModel.NodeType = NodeType;
			NodeModel.NodePosition = Position;
			return NodeModel.NodeGuid;
		}

		static void AddLink(
			FLbbLayeredBlendBodyPartGraphModel& GraphModel,
			const FGuid& FromNodeGuid,
			const FName& FromPinName,
			const FGuid& ToNodeGuid,
			const FName& ToPinName)
		{
			FLbbLayeredBlendBodyGraphLinkModel& Link = GraphModel.Links.AddDefaulted_GetRef();
			Link.FromNodeGuid = FromNodeGuid;
			Link.FromPinName = FromPinName;
			Link.ToNodeGuid = ToNodeGuid;
			Link.ToPinName = ToPinName;
		}

		static FName MakeTempPoseName(const int32 TempIndex)
		{
			return FName(*FString::Printf(TEXT("__GraphTemp_%d"), TempIndex));
		}

		static FLbbLayeredBodyPartPoseSource MakeSourceFromFixedNodeType(const ELbbLayeredBlendBodyGraphNodeType NodeType)
		{
			FLbbLayeredBodyPartPoseSource SourcePose;

			switch (NodeType)
			{
			case ELbbLayeredBlendBodyGraphNodeType::CurrentPose:
				SourcePose.Type = ELbbLayeredBodyPartPoseSourceType::OutputPose;
				break;
			case ELbbLayeredBlendBodyGraphNodeType::Motion:
				SourcePose.Type = ELbbLayeredBodyPartPoseSourceType::Motion;
				break;
			case ELbbLayeredBlendBodyGraphNodeType::BasePose:
				SourcePose.Type = ELbbLayeredBodyPartPoseSourceType::BasePose;
				break;
			case ELbbLayeredBlendBodyGraphNodeType::OverlayPose:
				SourcePose.Type = ELbbLayeredBodyPartPoseSourceType::OverlayPose;
				break;
			default:
				SourcePose.Type = ELbbLayeredBodyPartPoseSourceType::OutputPose;
				break;
			}

			return SourcePose;
		}

		static FLbbLayeredBodyPartPoseTarget MakeOutputTarget()
		{
			FLbbLayeredBodyPartPoseTarget Target;
			Target.Type = ELbbLayeredBodyPartPoseTargetType::OutputPose;
			return Target;
		}

		static FLbbLayeredBodyPartPoseTarget MakeTemporaryTarget(const int32 TempIndex)
		{
			FLbbLayeredBodyPartPoseTarget Target;
			Target.Type = ELbbLayeredBodyPartPoseTargetType::TemporaryPose;
			Target.TemporaryPoseName = MakeTempPoseName(TempIndex);
			return Target;
		}

		static FLbbLayeredBodyPartPoseSource MakeTemporarySource(const int32 TempIndex)
		{
			FLbbLayeredBodyPartPoseSource Source;
			Source.Type = ELbbLayeredBodyPartPoseSourceType::TemporaryPose;
			Source.TemporaryPoseName = MakeTempPoseName(TempIndex);
			return Source;
		}

		static bool BuildGraphContext(
			const FLbbLayeredBlendBodyPartGraphModel& GraphModel,
			const int32 BodyPartIndex,
			TArray<FCompileMessage>& OutMessages,
			FGraphContext& OutGraphContext)
		{
			TMap<ELbbLayeredBlendBodyGraphNodeType, int32> FixedNodeCounts;

			for (const FLbbLayeredBlendBodyGraphNodeModel& NodeModel : GraphModel.Nodes)
			{
				if (!NodeModel.NodeGuid.IsValid())
				{
					AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("NodeGuid is invalid."));
					continue;
				}

				if (OutGraphContext.NodesByGuid.Contains(NodeModel.NodeGuid))
				{
					AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Duplicate NodeGuid detected."));
					continue;
				}

				OutGraphContext.NodesByGuid.Add(NodeModel.NodeGuid, &NodeModel);

				switch (NodeModel.NodeType)
				{
				case ELbbLayeredBlendBodyGraphNodeType::CurrentPose:
					OutGraphContext.CurrentPoseNode = &NodeModel;
					FixedNodeCounts.FindOrAdd(NodeModel.NodeType)++;
					break;
				case ELbbLayeredBlendBodyGraphNodeType::Motion:
					OutGraphContext.MotionNode = &NodeModel;
					FixedNodeCounts.FindOrAdd(NodeModel.NodeType)++;
					break;
				case ELbbLayeredBlendBodyGraphNodeType::BasePose:
					OutGraphContext.BasePoseNode = &NodeModel;
					FixedNodeCounts.FindOrAdd(NodeModel.NodeType)++;
					break;
				case ELbbLayeredBlendBodyGraphNodeType::OverlayPose:
					OutGraphContext.OverlayPoseNode = &NodeModel;
					FixedNodeCounts.FindOrAdd(NodeModel.NodeType)++;
					break;
				case ELbbLayeredBlendBodyGraphNodeType::Result:
					OutGraphContext.ResultNode = &NodeModel;
					FixedNodeCounts.FindOrAdd(NodeModel.NodeType)++;
					break;
				default:
					break;
				}
			}

			auto ValidateFixedNodeCount = [&OutMessages, BodyPartIndex, &FixedNodeCounts](const ELbbLayeredBlendBodyGraphNodeType NodeType, const TCHAR* NodeDisplayName)
			{
				const int32 Count = FixedNodeCounts.FindRef(NodeType);
				if (Count != 1)
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						BodyPartIndex,
						FGuid(),
						FString::Printf(TEXT("Expected exactly one '%s' node, found %d."), NodeDisplayName, Count));
				}
			};

			ValidateFixedNodeCount(ELbbLayeredBlendBodyGraphNodeType::CurrentPose, TEXT("Current Pose"));
			ValidateFixedNodeCount(ELbbLayeredBlendBodyGraphNodeType::Motion, TEXT("Motion"));
			ValidateFixedNodeCount(ELbbLayeredBlendBodyGraphNodeType::BasePose, TEXT("Base Pose"));
			ValidateFixedNodeCount(ELbbLayeredBlendBodyGraphNodeType::OverlayPose, TEXT("Overlay Pose"));
			ValidateFixedNodeCount(ELbbLayeredBlendBodyGraphNodeType::Result, TEXT("Result"));

			for (const FLbbLayeredBlendBodyGraphLinkModel& LinkModel : GraphModel.Links)
			{
				const FLbbLayeredBlendBodyGraphNodeModel* const* FromNodePtr = OutGraphContext.NodesByGuid.Find(LinkModel.FromNodeGuid);
				const FLbbLayeredBlendBodyGraphNodeModel* const* ToNodePtr = OutGraphContext.NodesByGuid.Find(LinkModel.ToNodeGuid);
				if (FromNodePtr == nullptr || ToNodePtr == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, FGuid(), TEXT("A link references a missing node."));
					continue;
				}

				if (!IsValidOutputPin((*FromNodePtr)->NodeType, LinkModel.FromPinName))
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						BodyPartIndex,
						LinkModel.FromNodeGuid,
						FString::Printf(TEXT("Invalid output pin '%s'."), *LinkModel.FromPinName.ToString()));
					continue;
				}

				if (!IsValidInputPin((*ToNodePtr)->NodeType, LinkModel.ToPinName))
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						BodyPartIndex,
						LinkModel.ToNodeGuid,
						FString::Printf(TEXT("Invalid input pin '%s'."), *LinkModel.ToPinName.ToString()));
					continue;
				}

				const FPinTargetKey InputKey{LinkModel.ToNodeGuid, LinkModel.ToPinName};
				if (OutGraphContext.InputLinks.Contains(InputKey))
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						BodyPartIndex,
						LinkModel.ToNodeGuid,
						FString::Printf(TEXT("Input pin '%s' has more than one connection."), *LinkModel.ToPinName.ToString()));
					continue;
				}

				OutGraphContext.InputLinks.Add(InputKey, &LinkModel);
			}

			return !HasErrors(OutMessages);
		}

		static bool VisitNode(
			const FGraphContext& GraphContext,
			const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
			const int32 BodyPartIndex,
			TSet<FGuid>& ReachableNodes,
			TSet<FGuid>& VisitingNodes,
			TSet<FGuid>& VisitedNodes,
			TArray<FGuid>& OutTopoAuthorNodes,
			TArray<FCompileMessage>& OutMessages)
		{
			if (VisitedNodes.Contains(NodeModel.NodeGuid))
			{
				return true;
			}

			if (VisitingNodes.Contains(NodeModel.NodeGuid))
			{
				AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Cycle detected in body part graph."));
				return false;
			}

			VisitingNodes.Add(NodeModel.NodeGuid);
			ReachableNodes.Add(NodeModel.NodeGuid);

			TArray<FName, TInlineAllocator<2>> RequiredInputPins;
			GetInputPinNames(NodeModel.NodeType, RequiredInputPins);

			for (const FName& InputPinName : RequiredInputPins)
			{
				const FLbbLayeredBlendBodyGraphLinkModel* InputLink = FindInputLink(GraphContext, NodeModel.NodeGuid, InputPinName);
				if (InputLink == nullptr)
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						BodyPartIndex,
						NodeModel.NodeGuid,
						FString::Printf(TEXT("Required input pin '%s' is not connected."), *InputPinName.ToString()));
					continue;
				}

				const FLbbLayeredBlendBodyGraphNodeModel* const* SourceNodePtr = GraphContext.NodesByGuid.Find(InputLink->FromNodeGuid);
				if (SourceNodePtr == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Input link references a missing source node."));
					continue;
				}

				VisitNode(GraphContext, **SourceNodePtr, BodyPartIndex, ReachableNodes, VisitingNodes, VisitedNodes, OutTopoAuthorNodes, OutMessages);
			}

			VisitingNodes.Remove(NodeModel.NodeGuid);
			VisitedNodes.Add(NodeModel.NodeGuid);

			if (IsAuthorNodeType(NodeModel.NodeType))
			{
				OutTopoAuthorNodes.Add(NodeModel.NodeGuid);
			}

			return true;
		}

		static bool BuildReachableGraph(
			const FGraphContext& GraphContext,
			const int32 BodyPartIndex,
			TSet<FGuid>& OutReachableNodes,
			TArray<FGuid>& OutTopoAuthorNodes,
			FGuid& OutFinalSourceGuid,
			TArray<FCompileMessage>& OutMessages)
		{
			check(GraphContext.ResultNode != nullptr);

			const FLbbLayeredBlendBodyGraphLinkModel* ResultInputLink = FindInputLink(GraphContext, GraphContext.ResultNode->NodeGuid, PinPose);
			if (ResultInputLink == nullptr)
			{
				AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, GraphContext.ResultNode->NodeGuid, TEXT("Result node is not connected."));
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeModel* const* FinalSourceNodePtr = GraphContext.NodesByGuid.Find(ResultInputLink->FromNodeGuid);
			if (FinalSourceNodePtr == nullptr)
			{
				AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, GraphContext.ResultNode->NodeGuid, TEXT("Result node references a missing source node."));
				return false;
			}

			OutFinalSourceGuid = (*FinalSourceNodePtr)->NodeGuid;

			TSet<FGuid> VisitingNodes;
			TSet<FGuid> VisitedNodes;
			VisitNode(GraphContext, **FinalSourceNodePtr, BodyPartIndex, OutReachableNodes, VisitingNodes, VisitedNodes, OutTopoAuthorNodes, OutMessages);

			for (const FLbbLayeredBlendBodyGraphNodeModel& NodeModel : GraphContext.GraphModel.Nodes)
			{
				if (!OutReachableNodes.Contains(NodeModel.NodeGuid))
				{
					if (!IsFixedNodeType(NodeModel.NodeType)
						&& NodeModel.NodeType != ELbbLayeredBlendBodyGraphNodeType::Result)
					{
						AddMessage(
							OutMessages,
							EMessageSeverity::Warning,
							BodyPartIndex,
							NodeModel.NodeGuid,
							TEXT("Dead node is not connected to Result."));
					}
				}
			}

			return !HasErrors(OutMessages);
		}

		static bool TryResolveCompiledSource(
			const FGraphContext& GraphContext,
			const TMap<FGuid, int32>& TempPoseIndices,
			const FGuid& SourceNodeGuid,
			FLbbLayeredBodyPartPoseSource& OutSourcePose)
		{
			const FLbbLayeredBlendBodyGraphNodeModel* const* SourceNodePtr = GraphContext.NodesByGuid.Find(SourceNodeGuid);
			if (SourceNodePtr == nullptr)
			{
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeModel& SourceNode = **SourceNodePtr;
			if (IsFixedNodeType(SourceNode.NodeType))
			{
				OutSourcePose = MakeSourceFromFixedNodeType(SourceNode.NodeType);
				return true;
			}

			if (const int32* TempPoseIndex = TempPoseIndices.Find(SourceNodeGuid))
			{
				OutSourcePose = MakeTemporarySource(*TempPoseIndex);
				return true;
			}

			return false;
		}

		template <typename OperatorType>
		static void AddOperatorStruct(TArray<FInstancedStruct>& Operators, const OperatorType& Operator)
		{
			FInstancedStruct& OperatorData = Operators.AddDefaulted_GetRef();
			OperatorData.InitializeAs<OperatorType>();
			OperatorData.GetMutable<OperatorType>() = Operator;
		}

		static bool CompileBodyPartGraph(
			const FLbbLayeredBlendBodyPartGraphModel& GraphModel,
			const int32 BodyPartIndex,
			FLbbLayeredBlendBodyPart& OutBodyPart,
			TArray<FCompileMessage>& OutMessages)
		{
			OutBodyPart.PartName = GraphModel.PartName;
#if WITH_EDITORONLY_DATA
			OutBodyPart.DebugColor = GraphModel.DebugColor;
#endif

			FGraphContext GraphContext(GraphModel);
			BuildGraphContext(GraphModel, BodyPartIndex, OutMessages, GraphContext);
			if (HasErrors(OutMessages))
			{
				return false;
			}

			TSet<FGuid> ReachableNodes;
			TArray<FGuid> TopoAuthorNodes;
			FGuid FinalSourceGuid;
			BuildReachableGraph(GraphContext, BodyPartIndex, ReachableNodes, TopoAuthorNodes, FinalSourceGuid, OutMessages);
			if (HasErrors(OutMessages))
			{
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeModel* const* FinalSourceNodePtr = GraphContext.NodesByGuid.Find(FinalSourceGuid);
			if (FinalSourceNodePtr == nullptr)
			{
				AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, FinalSourceGuid, TEXT("Final source node is missing."));
				return false;
			}

			TMap<FGuid, int32> TempPoseIndices;
			int32 NextTempPoseIndex = 0;
			for (const FGuid& NodeGuid : TopoAuthorNodes)
			{
				if (NodeGuid != FinalSourceGuid)
				{
					TempPoseIndices.Add(NodeGuid, NextTempPoseIndex++);
				}
			}

			for (const FGuid& NodeGuid : TopoAuthorNodes)
			{
				const FLbbLayeredBlendBodyGraphNodeModel* const* NodePtr = GraphContext.NodesByGuid.Find(NodeGuid);
				if (NodePtr == nullptr)
				{
					continue;
				}

				const FLbbLayeredBlendBodyGraphNodeModel& NodeModel = **NodePtr;
				const FLbbLayeredBodyPartPoseTarget TargetPose = (NodeGuid == FinalSourceGuid)
					? MakeOutputTarget()
					: MakeTemporaryTarget(TempPoseIndices.FindChecked(NodeGuid));

				switch (NodeModel.NodeType)
				{
				case ELbbLayeredBlendBodyGraphNodeType::ApplySlot:
				{
					const FLbbLayeredBlendBodyGraphLinkModel* InputLink = FindInputLink(GraphContext, NodeModel.NodeGuid, PinInput);
					FLbbLayeredBodyPartPoseSource SourcePose;
					if (InputLink == nullptr || !TryResolveCompiledSource(GraphContext, TempPoseIndices, InputLink->FromNodeGuid, SourcePose))
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Apply Slot failed to resolve its input."));
						return false;
					}

					const FLbbLayeredBlendBodyGraphNodeData_ApplySlot* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>();
					if (NodeData == nullptr || NodeData->SlotName.IsNone())
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Apply Slot requires a valid SlotName."));
						return false;
					}

					FLbbLayeredBlendBodyPartOperator_ApplySlot Operator;
					Operator.SourcePose = SourcePose;
					Operator.SlotName = NodeData->SlotName;
					Operator.Output = TargetPose;
					AddOperatorStruct(OutBodyPart.Operators, Operator);
					break;
				}
				case ELbbLayeredBlendBodyGraphNodeType::Blend:
				{
					const FLbbLayeredBlendBodyGraphLinkModel* BaseLink = FindInputLink(GraphContext, NodeModel.NodeGuid, PinBase);
					const FLbbLayeredBlendBodyGraphLinkModel* BlendLink = FindInputLink(GraphContext, NodeModel.NodeGuid, PinBlend);
					FLbbLayeredBodyPartPoseSource BaseSource;
					FLbbLayeredBodyPartPoseSource BlendSource;
					if (BaseLink == nullptr || BlendLink == nullptr
						|| !TryResolveCompiledSource(GraphContext, TempPoseIndices, BaseLink->FromNodeGuid, BaseSource)
						|| !TryResolveCompiledSource(GraphContext, TempPoseIndices, BlendLink->FromNodeGuid, BlendSource))
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Blend failed to resolve one of its inputs."));
						return false;
					}

					const FLbbLayeredBlendBodyGraphNodeData_Blend* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_Blend>();
					if (NodeData == nullptr)
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Blend node data is missing."));
						return false;
					}

					if (NodeData->Weight.bUseBlendCurve && NodeData->Weight.BlendCurveName.IsNone())
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Blend uses a curve weight but BlendCurveName is None."));
						return false;
					}

					FLbbLayeredBlendBodyPartOperator_BlendTwoPoses Operator;
					Operator.BasePose = BaseSource;
					Operator.BlendPose = BlendSource;
					Operator.Weight = NodeData->Weight;
					Operator.Output = TargetPose;
					AddOperatorStruct(OutBodyPart.Operators, Operator);
					break;
				}
				case ELbbLayeredBlendBodyGraphNodeType::MaskedBlend:
				{
					const FLbbLayeredBlendBodyGraphLinkModel* BaseLink = FindInputLink(GraphContext, NodeModel.NodeGuid, PinBase);
					const FLbbLayeredBlendBodyGraphLinkModel* BlendLink = FindInputLink(GraphContext, NodeModel.NodeGuid, PinBlend);
					FLbbLayeredBodyPartPoseSource BaseSource;
					FLbbLayeredBodyPartPoseSource BlendSource;
					if (BaseLink == nullptr || BlendLink == nullptr
						|| !TryResolveCompiledSource(GraphContext, TempPoseIndices, BaseLink->FromNodeGuid, BaseSource)
						|| !TryResolveCompiledSource(GraphContext, TempPoseIndices, BlendLink->FromNodeGuid, BlendSource))
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Masked Blend failed to resolve one of its inputs."));
						return false;
					}

					const FLbbLayeredBlendBodyGraphNodeData_MaskedBlend* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_MaskedBlend>();
					if (NodeData == nullptr)
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Masked Blend node data is missing."));
						return false;
					}

					if (NodeData->BoneFilter.BranchFilters.IsEmpty())
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Masked Blend requires a non-empty BoneFilter."));
						return false;
					}

					if (NodeData->Weight.bUseBlendCurve && NodeData->Weight.BlendCurveName.IsNone())
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Masked Blend uses a curve weight but BlendCurveName is None."));
						return false;
					}

					FLbbLayeredBlendBodyPartOperator_MaskedBlend Operator;
					Operator.BasePose = BaseSource;
					Operator.BlendPose = BlendSource;
					Operator.BlendSpace = NodeData->BlendSpace;
					Operator.BoneFilter = NodeData->BoneFilter;
					Operator.Weight = NodeData->Weight;
					Operator.Output = TargetPose;
					AddOperatorStruct(OutBodyPart.Operators, Operator);
					break;
				}
				case ELbbLayeredBlendBodyGraphNodeType::ApplyMotionDelta:
				{
					const FLbbLayeredBlendBodyGraphLinkModel* BaseLink = FindInputLink(GraphContext, NodeModel.NodeGuid, PinBase);
					FLbbLayeredBodyPartPoseSource BaseSource;
					if (BaseLink == nullptr || !TryResolveCompiledSource(GraphContext, TempPoseIndices, BaseLink->FromNodeGuid, BaseSource))
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Apply Motion Delta failed to resolve its base input."));
						return false;
					}

					const FLbbLayeredBlendBodyGraphNodeData_ApplyMotionDelta* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplyMotionDelta>();
					if (NodeData == nullptr)
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Apply Motion Delta node data is missing."));
						return false;
					}

					if (NodeData->Weight.bUseBlendCurve && NodeData->Weight.BlendCurveName.IsNone())
					{
						AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Apply Motion Delta uses a curve weight but BlendCurveName is None."));
						return false;
					}

					FLbbLayeredBlendBodyPartOperator_ApplyAdditive Operator;
					Operator.BasePose = BaseSource;
					Operator.AdditiveSpace = NodeData->AdditiveSpace;
					Operator.Weight = NodeData->Weight;
					Operator.Output = TargetPose;
					AddOperatorStruct(OutBodyPart.Operators, Operator);
					break;
				}
				default:
					AddMessage(OutMessages, EMessageSeverity::Error, BodyPartIndex, NodeModel.NodeGuid, TEXT("Unsupported author node type."));
					return false;
				}
			}

			const FLbbLayeredBlendBodyGraphNodeModel& FinalSourceNode = **FinalSourceNodePtr;
			if (!IsAuthorNodeType(FinalSourceNode.NodeType))
			{
				if (FinalSourceNode.NodeType != ELbbLayeredBlendBodyGraphNodeType::CurrentPose)
				{
					FLbbLayeredBlendBodyPartOperator_CopyPose Operator;
					Operator.SourcePose = MakeSourceFromFixedNodeType(FinalSourceNode.NodeType);
					Operator.Output = MakeOutputTarget();
					AddOperatorStruct(OutBodyPart.Operators, Operator);
				}
			}

			return true;
		}
	}

	void FLbbLayeredBlendBodyGraphCompiler::CreateDefaultBodyPartGraph(
		FLbbLayeredBlendBodyPartGraphModel& OutGraphModel,
		const FName& PartName)
	{
		OutGraphModel = FLbbLayeredBlendBodyPartGraphModel();
		OutGraphModel.GraphGuid = FGuid::NewGuid();
		OutGraphModel.PartName = PartName;

		const FGuid CurrentPoseGuid = AddNode(OutGraphModel, ELbbLayeredBlendBodyGraphNodeType::CurrentPose, FVector2D(0.f, 0.f));
		AddNode(OutGraphModel, ELbbLayeredBlendBodyGraphNodeType::Motion, FVector2D(0.f, 180.f));
		AddNode(OutGraphModel, ELbbLayeredBlendBodyGraphNodeType::BasePose, FVector2D(0.f, 360.f));
		AddNode(OutGraphModel, ELbbLayeredBlendBodyGraphNodeType::OverlayPose, FVector2D(0.f, 540.f));
		const FGuid ResultGuid = AddNode(OutGraphModel, ELbbLayeredBlendBodyGraphNodeType::Result, FVector2D(920.f, 180.f));

		AddLink(OutGraphModel, CurrentPoseGuid, PinPose, ResultGuid, PinPose);
	}

	FCompileResult FLbbLayeredBlendBodyGraphCompiler::Compile(ULbbLayeredBlendBodyDefinition& Definition)
	{
		FCompileResult Result;
		Result.bSuccess = false;

#if !WITH_EDITORONLY_DATA
		Result.bSuccess = true;
		return Result;
#else
		TArray<FLbbLayeredBlendBodyPart> CompiledBodyParts;
		CompiledBodyParts.Reserve(Definition.EditorModel.BodyPartGraphs.Num());

		for (int32 BodyPartIndex = 0; BodyPartIndex < Definition.EditorModel.BodyPartGraphs.Num(); ++BodyPartIndex)
		{
			const FLbbLayeredBlendBodyPartGraphModel& GraphModel = Definition.EditorModel.BodyPartGraphs[BodyPartIndex];
			FLbbLayeredBlendBodyPart& CompiledBodyPart = CompiledBodyParts.AddDefaulted_GetRef();
			CompileBodyPartGraph(GraphModel, BodyPartIndex, CompiledBodyPart, Result.Messages);
		}

		TMap<FName, int32> PartNameCounts;
		for (int32 BodyPartIndex = 0; BodyPartIndex < Definition.EditorModel.BodyPartGraphs.Num(); ++BodyPartIndex)
		{
			const FName PartName = Definition.EditorModel.BodyPartGraphs[BodyPartIndex].PartName;
			if (!PartName.IsNone())
			{
				PartNameCounts.FindOrAdd(PartName)++;
			}
		}

		for (int32 BodyPartIndex = 0; BodyPartIndex < Definition.EditorModel.BodyPartGraphs.Num(); ++BodyPartIndex)
		{
			const FName PartName = Definition.EditorModel.BodyPartGraphs[BodyPartIndex].PartName;
			if (!PartName.IsNone() && PartNameCounts.FindRef(PartName) > 1)
			{
				AddMessage(
					Result.Messages,
					EMessageSeverity::Warning,
					BodyPartIndex,
					FGuid(),
					FString::Printf(TEXT("BodyPart name '%s' is duplicated."), *PartName.ToString()));
			}
		}

		Result.bSuccess = !HasErrors(Result.Messages);

		if (Result.bSuccess)
		{
			Definition.BodyParts = MoveTemp(CompiledBodyParts);
		}

		return Result;
#endif
	}
}
