// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbLayeredBlendBodyGraphCompiler.h"

#include "LbbLayeredBlendBodyDefinition.h"
#include "LbbLayeredBlendBodyGraphNodeRegistry.h"
#include "LbbLayeredBlendBodyOperators.h"

namespace
{
	const FName PinPose(TEXT("Pose"));
	const FName PinInput(TEXT("Input"));
	static constexpr TCHAR InternalCachePosePrefix[] = TEXT("__LbbInternalCache_");

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
		const FLbbLayeredBlendBodyGraphModelBase& GraphModel;
		ELbbLayeredBlendBodyGraphKind GraphKind = ELbbLayeredBlendBodyGraphKind::BodyPart;
		TMap<FGuid, const FLbbLayeredBlendBodyGraphNodeModel*> NodesByGuid;
		TMap<FGuid, const FLbbLayeredBlendBodyGraphNodeDescriptor*> DescriptorsByGuid;
		TMap<FPinTargetKey, const FLbbLayeredBlendBodyGraphLinkModel*> InputLinks;
		const FLbbLayeredBlendBodyGraphNodeModel* ResultNode = nullptr;
		TArray<const FLbbLayeredBlendBodyGraphNodeModel*> SavePoseNodes;

		FGraphContext(const FLbbLayeredBlendBodyGraphModelBase& InGraphModel, const ELbbLayeredBlendBodyGraphKind InGraphKind)
			: GraphModel(InGraphModel)
			, GraphKind(InGraphKind)
		{
		}
	};

	struct FCompiledCacheGraph
	{
		TArray<FName> NamedCacheNames;
		TArray<FInstancedStruct> Operators;
	};

	static bool HasErrors(const TArray<FLbbCompileMessage>& Messages)
	{
		for (const FLbbCompileMessage& Message : Messages)
		{
			if (Message.Severity == EMessageSeverity::Error)
			{
				return true;
			}
		}

		return false;
	}

	static void AddMessage(
		TArray<FLbbCompileMessage>& OutMessages,
		const EMessageSeverity::Type Severity,
		const ELbbLayeredBlendBodyGraphKind GraphKind,
		const int32 BodyPartIndex,
		const FGuid& NodeGuid,
		const FString& Message)
	{
		FLbbCompileMessage& NewMessage = OutMessages.AddDefaulted_GetRef();
		NewMessage.Severity = Severity;
		NewMessage.GraphKind = GraphKind;
		NewMessage.BodyPartIndex = BodyPartIndex;
		NewMessage.NodeGuid = NodeGuid;
		NewMessage.Message = Message;
	}

	static const FLbbLayeredBlendBodyGraphNodeDescriptor* FindDescriptor(
		const FGraphContext& GraphContext,
		const FGuid& NodeGuid)
	{
		return GraphContext.DescriptorsByGuid.FindRef(NodeGuid);
	}

	static bool IsValidInputPin(const FLbbLayeredBlendBodyGraphNodeDescriptor& Descriptor, const FName PinName)
	{
		return Descriptor.GetInputPins().Contains(PinName);
	}

	static bool IsValidOutputPin(const FLbbLayeredBlendBodyGraphNodeDescriptor& Descriptor, const FName PinName)
	{
		return Descriptor.GetOutputPins().Contains(PinName);
	}

	static const FLbbLayeredBlendBodyGraphLinkModel* FindInputLink(
		const FGraphContext& GraphContext,
			const FGuid& NodeGuid,
			const FName& PinName)
		{
			return GraphContext.InputLinks.FindRef(FPinTargetKey{NodeGuid, PinName});
		}

		template <typename NodeDataType>
		static FGuid AddNode(
			FLbbLayeredBlendBodyGraphModelBase& GraphModel,
			const FVector2D& Position,
			TFunctionRef<void(NodeDataType&)> InitializeNode)
		{
			FLbbLayeredBlendBodyGraphNodeModel& NodeModel = GraphModel.Nodes.AddDefaulted_GetRef();
			NodeModel.NodeGuid = FGuid::NewGuid();
			NodeModel.NodePosition = Position;
			NodeModel.NodeData.InitializeAs<NodeDataType>();
			InitializeNode(NodeModel.NodeData.GetMutable<NodeDataType>());
			return NodeModel.NodeGuid;
		}

		static void AddLink(
			FLbbLayeredBlendBodyGraphModelBase& GraphModel,
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

		static bool IsAllowedInputSource(
			const ELbbLayeredBlendBodyGraphKind GraphKind,
			const ELbbLayeredBodyPartPoseSourceType SourceType)
		{
			if (GraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
			{
				return SourceType == ELbbLayeredBodyPartPoseSourceType::Motion
					|| SourceType == ELbbLayeredBodyPartPoseSourceType::BasePose
					|| SourceType == ELbbLayeredBodyPartPoseSourceType::OverlayPose;
			}

			return true;
		}

		static bool IsReservedInternalCachePoseName(const FName CachePoseName)
		{
			return CachePoseName.ToString().StartsWith(InternalCachePosePrefix);
		}

		static FString BuildInternalCacheScopeName(
			const ELbbLayeredBlendBodyGraphKind GraphKind,
			const int32 BodyPartIndex)
		{
			return GraphKind == ELbbLayeredBlendBodyGraphKind::Cache
				? TEXT("CacheGraph")
				: FString::Printf(TEXT("BodyPart_%d"), BodyPartIndex);
		}

		static FName MakeInternalCachePoseName(const FString& ScopeName, const int32 CacheIndex)
		{
			return FName(*FString::Printf(TEXT("%s%s_%d"), InternalCachePosePrefix, *ScopeName, CacheIndex));
		}

		static FName AllocateInternalCachePoseName(
			const FString& ScopeName,
			const int32 SuggestedIndex,
			const TSet<FName>& ReservedUserCacheNames,
			TSet<FName>& UsedInternalCacheNames)
		{
			int32 CandidateIndex = SuggestedIndex;
			while (true)
			{
				const FName CandidateName = MakeInternalCachePoseName(ScopeName, CandidateIndex);
				if (!ReservedUserCacheNames.Contains(CandidateName) && !UsedInternalCacheNames.Contains(CandidateName))
				{
					UsedInternalCacheNames.Add(CandidateName);
					return CandidateName;
				}

				++CandidateIndex;
			}
		}

		static FLbbLayeredBodyPartPoseTarget MakeCurrentPoseTarget()
		{
			FLbbLayeredBodyPartPoseTarget Target;
			Target.Type = ELbbLayeredBodyPartPoseTargetType::CurrentPose;
			return Target;
		}

		static FLbbLayeredBodyPartPoseTarget MakeCachePoseTarget(const FName CachePoseName)
		{
			FLbbLayeredBodyPartPoseTarget Target;
			Target.Type = ELbbLayeredBodyPartPoseTargetType::CachePose;
			Target.CachePoseName = CachePoseName;
			return Target;
		}

		static FLbbLayeredBodyPartPoseSource MakeCachePoseSource(const FName CachePoseName)
		{
			FLbbLayeredBodyPartPoseSource Source;
			Source.Type = ELbbLayeredBodyPartPoseSourceType::CachePose;
			Source.CachePoseName = CachePoseName;
			return Source;
		}

		static bool BuildGraphContext(
			const FLbbLayeredBlendBodyGraphModelBase& GraphModel,
			const ELbbLayeredBlendBodyGraphKind GraphKind,
			const int32 BodyPartIndex,
			TArray<FLbbCompileMessage>& OutMessages,
			FGraphContext& OutGraphContext)
		{
			TMap<const UScriptStruct*, int32> SingletonNodeCounts;

			for (const FLbbLayeredBlendBodyGraphNodeModel& NodeModel : GraphModel.Nodes)
			{
				if (!NodeModel.NodeGuid.IsValid())
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphKind, BodyPartIndex, NodeModel.NodeGuid, TEXT("NodeGuid is invalid."));
					continue;
				}

				if (OutGraphContext.NodesByGuid.Contains(NodeModel.NodeGuid))
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphKind, BodyPartIndex, NodeModel.NodeGuid, TEXT("Duplicate NodeGuid detected."));
					continue;
				}

				const UScriptStruct* NodeDataStruct = NodeModel.NodeData.GetScriptStruct();
				const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindLbbLayeredBlendBodyGraphNodeDescriptor(NodeDataStruct);
				if (Descriptor == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphKind, BodyPartIndex, NodeModel.NodeGuid, TEXT("NodeData uses an unsupported graph node struct."));
					continue;
				}

				if (!Descriptor->IsAllowedInGraph(GraphKind))
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						GraphKind,
						BodyPartIndex,
						NodeModel.NodeGuid,
						FString::Printf(TEXT("Node '%s' is not allowed in this graph."), Descriptor->DisplayName));
					continue;
				}

				OutGraphContext.NodesByGuid.Add(NodeModel.NodeGuid, &NodeModel);
				OutGraphContext.DescriptorsByGuid.Add(NodeModel.NodeGuid, Descriptor);

				if (Descriptor->bIsResultNode)
				{
					OutGraphContext.ResultNode = &NodeModel;
				}

				if (Descriptor->bIsSavePoseNode)
				{
					OutGraphContext.SavePoseNodes.Add(&NodeModel);
				}

				if (Descriptor->IsSingletonInGraph(GraphKind))
				{
					SingletonNodeCounts.FindOrAdd(Descriptor->NodeDataStruct)++;
				}
			}

			for (const FLbbLayeredBlendBodyGraphNodeDescriptor& Descriptor : GetLbbLayeredBlendBodyGraphNodeDescriptors())
			{
				if (!Descriptor.IsSingletonInGraph(GraphKind))
				{
					continue;
				}

				const int32 Count = SingletonNodeCounts.FindRef(Descriptor.NodeDataStruct);
				if (Count != 1)
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						GraphKind,
						BodyPartIndex,
						FGuid(),
						FString::Printf(TEXT("Expected exactly one '%s' node, found %d."), Descriptor.DisplayName, Count));
				}
			}

			if (GraphKind == ELbbLayeredBlendBodyGraphKind::Cache && GraphModel.Nodes.Num() > 0 && OutGraphContext.SavePoseNodes.IsEmpty())
			{
				AddMessage(OutMessages, EMessageSeverity::Error, GraphKind, BodyPartIndex, FGuid(), TEXT("Cache Graph requires at least one Save Pose node."));
			}

			for (const FLbbLayeredBlendBodyGraphLinkModel& LinkModel : GraphModel.Links)
			{
				const FLbbLayeredBlendBodyGraphNodeModel* const* FromNodePtr = OutGraphContext.NodesByGuid.Find(LinkModel.FromNodeGuid);
				const FLbbLayeredBlendBodyGraphNodeModel* const* ToNodePtr = OutGraphContext.NodesByGuid.Find(LinkModel.ToNodeGuid);
				if (FromNodePtr == nullptr || ToNodePtr == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphKind, BodyPartIndex, FGuid(), TEXT("A link references a missing node."));
					continue;
				}

				const FLbbLayeredBlendBodyGraphNodeDescriptor* FromDescriptor = FindDescriptor(OutGraphContext, LinkModel.FromNodeGuid);
				const FLbbLayeredBlendBodyGraphNodeDescriptor* ToDescriptor = FindDescriptor(OutGraphContext, LinkModel.ToNodeGuid);
				if (FromDescriptor == nullptr || ToDescriptor == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphKind, BodyPartIndex, FGuid(), TEXT("A link references an unsupported node."));
					continue;
				}

				if (!IsValidOutputPin(*FromDescriptor, LinkModel.FromPinName))
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						GraphKind,
						BodyPartIndex,
						LinkModel.FromNodeGuid,
						FString::Printf(TEXT("Invalid output pin '%s'."), *LinkModel.FromPinName.ToString()));
					continue;
				}

				if (!IsValidInputPin(*ToDescriptor, LinkModel.ToPinName))
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						GraphKind,
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
						GraphKind,
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
			TArray<FLbbCompileMessage>& OutMessages)
		{
			if (VisitedNodes.Contains(NodeModel.NodeGuid))
			{
				return true;
			}

			if (VisitingNodes.Contains(NodeModel.NodeGuid))
			{
				AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, NodeModel.NodeGuid, TEXT("Cycle detected in graph."));
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindDescriptor(GraphContext, NodeModel.NodeGuid);
			if (Descriptor == nullptr)
			{
				AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, NodeModel.NodeGuid, TEXT("Node descriptor is missing."));
				return false;
			}

			VisitingNodes.Add(NodeModel.NodeGuid);
			ReachableNodes.Add(NodeModel.NodeGuid);

			for (const FName& InputPinName : Descriptor->GetInputPins())
			{
				const FLbbLayeredBlendBodyGraphLinkModel* InputLink = FindInputLink(GraphContext, NodeModel.NodeGuid, InputPinName);
				if (InputLink == nullptr)
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						GraphContext.GraphKind,
						BodyPartIndex,
						NodeModel.NodeGuid,
						FString::Printf(TEXT("Required input pin '%s' is not connected."), *InputPinName.ToString()));
					continue;
				}

				const FLbbLayeredBlendBodyGraphNodeModel* const* SourceNodePtr = GraphContext.NodesByGuid.Find(InputLink->FromNodeGuid);
				if (SourceNodePtr == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, NodeModel.NodeGuid, TEXT("Input link references a missing source node."));
					continue;
				}

				VisitNode(GraphContext, **SourceNodePtr, BodyPartIndex, ReachableNodes, VisitingNodes, VisitedNodes, OutTopoAuthorNodes, OutMessages);
			}

			VisitingNodes.Remove(NodeModel.NodeGuid);
			VisitedNodes.Add(NodeModel.NodeGuid);

			if (Descriptor->IsAuthorNode())
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
			TArray<FLbbCompileMessage>& OutMessages)
		{
			TSet<FGuid> VisitingNodes;
			TSet<FGuid> VisitedNodes;

			if (GraphContext.GraphKind == ELbbLayeredBlendBodyGraphKind::BodyPart)
			{
				check(GraphContext.ResultNode != nullptr);

				const FLbbLayeredBlendBodyGraphLinkModel* ResultInputLink = FindInputLink(GraphContext, GraphContext.ResultNode->NodeGuid, PinPose);
				if (ResultInputLink == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, GraphContext.ResultNode->NodeGuid, TEXT("Result node is not connected."));
					return false;
				}

				const FLbbLayeredBlendBodyGraphNodeModel* const* FinalSourceNodePtr = GraphContext.NodesByGuid.Find(ResultInputLink->FromNodeGuid);
				if (FinalSourceNodePtr == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, GraphContext.ResultNode->NodeGuid, TEXT("Result node references a missing source node."));
					return false;
				}

				OutFinalSourceGuid = (*FinalSourceNodePtr)->NodeGuid;
				VisitNode(GraphContext, **FinalSourceNodePtr, BodyPartIndex, OutReachableNodes, VisitingNodes, VisitedNodes, OutTopoAuthorNodes, OutMessages);
			}
			else
			{
				for (const FLbbLayeredBlendBodyGraphNodeModel* SavePoseNode : GraphContext.SavePoseNodes)
				{
					if (SavePoseNode == nullptr)
					{
						continue;
					}

					const FLbbLayeredBlendBodyGraphLinkModel* InputLink = FindInputLink(GraphContext, SavePoseNode->NodeGuid, PinInput);
					if (InputLink == nullptr)
					{
						AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, SavePoseNode->NodeGuid, TEXT("Save Pose node is not connected."));
						continue;
					}

					const FLbbLayeredBlendBodyGraphNodeModel* const* SourceNodePtr = GraphContext.NodesByGuid.Find(InputLink->FromNodeGuid);
					if (SourceNodePtr == nullptr)
					{
						AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, SavePoseNode->NodeGuid, TEXT("Save Pose node references a missing source node."));
						continue;
					}

					OutReachableNodes.Add(SavePoseNode->NodeGuid);
					VisitNode(GraphContext, **SourceNodePtr, BodyPartIndex, OutReachableNodes, VisitingNodes, VisitedNodes, OutTopoAuthorNodes, OutMessages);
					if (!VisitedNodes.Contains(SavePoseNode->NodeGuid))
					{
						VisitedNodes.Add(SavePoseNode->NodeGuid);
						OutTopoAuthorNodes.Add(SavePoseNode->NodeGuid);
					}
				}
			}

			for (const FLbbLayeredBlendBodyGraphNodeModel& NodeModel : GraphContext.GraphModel.Nodes)
			{
				if (OutReachableNodes.Contains(NodeModel.NodeGuid))
				{
					continue;
				}

				const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindDescriptor(GraphContext, NodeModel.NodeGuid);
				if (Descriptor == nullptr || Descriptor->bIsResultNode)
				{
					continue;
				}

				AddMessage(
					OutMessages,
					EMessageSeverity::Warning,
					GraphContext.GraphKind,
					BodyPartIndex,
					NodeModel.NodeGuid,
					GraphContext.GraphKind == ELbbLayeredBlendBodyGraphKind::BodyPart
						? TEXT("Dead node is not connected to Result.")
						: TEXT("Dead node is not connected to any Save Pose node."));
			}

			return !HasErrors(OutMessages);
		}

		static bool TryResolveCompiledSource(
			const FGraphContext& GraphContext,
			const ELbbLayeredBlendBodyGraphKind GraphKind,
			const TMap<FGuid, FName>& InternalCachePoseNames,
			const TSet<FName>& ValidNamedCaches,
			const int32 BodyPartIndex,
			TArray<FLbbCompileMessage>& OutMessages,
			const FGuid& SourceNodeGuid,
			FLbbLayeredBodyPartPoseSource& OutSourcePose)
		{
			const FLbbLayeredBlendBodyGraphNodeModel* const* SourceNodePtr = GraphContext.NodesByGuid.Find(SourceNodeGuid);
			if (SourceNodePtr == nullptr)
			{
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindDescriptor(GraphContext, SourceNodeGuid);
			if (Descriptor == nullptr)
			{
				return false;
			}

			if (Descriptor->bIsInputNode)
			{
				const FLbbLayeredBlendBodyGraphNodeData_Input* NodeData = (*SourceNodePtr)->NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_Input>();
				if (NodeData == nullptr)
				{
					return false;
				}

				if (!IsAllowedInputSource(GraphKind, NodeData->SourcePose.Type))
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						GraphKind,
						BodyPartIndex,
						SourceNodeGuid,
						TEXT("Input node uses a source type that is not allowed in this graph."));
					return false;
				}

				if (NodeData->SourcePose.Type == ELbbLayeredBodyPartPoseSourceType::CachePose)
				{
					if (NodeData->SourcePose.CachePoseName.IsNone())
					{
						AddMessage(OutMessages, EMessageSeverity::Error, GraphKind, BodyPartIndex, SourceNodeGuid, TEXT("Input(CachePose) requires a valid CachePoseName."));
						return false;
					}

					if (!ValidNamedCaches.Contains(NodeData->SourcePose.CachePoseName))
					{
						AddMessage(
							OutMessages,
							EMessageSeverity::Error,
							GraphKind,
							BodyPartIndex,
							SourceNodeGuid,
							FString::Printf(TEXT("CachePose '%s' does not exist in Cache Graph."), *NodeData->SourcePose.CachePoseName.ToString()));
						return false;
					}
				}

				OutSourcePose = NodeData->SourcePose;
				return true;
			}

			if (const FName* InternalCachePoseName = InternalCachePoseNames.Find(SourceNodeGuid))
			{
				OutSourcePose = MakeCachePoseSource(*InternalCachePoseName);
				return true;
			}

			return false;
		}

		static bool CollectNamedCaches(
			const FGraphContext& GraphContext,
			const int32 BodyPartIndex,
			FCompiledCacheGraph& OutCacheGraph,
			TArray<FLbbCompileMessage>& OutMessages)
		{
			TSet<FName> SeenNames;
			for (const FLbbLayeredBlendBodyGraphNodeModel& NodeModel : GraphContext.GraphModel.Nodes)
			{
				const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindDescriptor(GraphContext, NodeModel.NodeGuid);
				if (Descriptor == nullptr || !Descriptor->bIsSavePoseNode)
				{
					continue;
				}

				const FLbbLayeredBlendBodyGraphNodeData_SavePose* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_SavePose>();
				if (NodeData == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, NodeModel.NodeGuid, TEXT("Save Pose node data is missing."));
					continue;
				}

				if (NodeData->CachePoseName.IsNone())
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, NodeModel.NodeGuid, TEXT("Save Pose requires a valid CachePoseName."));
					continue;
				}

				if (IsReservedInternalCachePoseName(NodeData->CachePoseName))
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Warning,
						GraphContext.GraphKind,
						BodyPartIndex,
						NodeModel.NodeGuid,
						TEXT("Save Pose uses a reserved internal cache prefix. The compiler will avoid collisions, but this name is reserved for internal slots."));
				}

				if (SeenNames.Contains(NodeData->CachePoseName))
				{
					AddMessage(
						OutMessages,
						EMessageSeverity::Error,
						GraphContext.GraphKind,
						BodyPartIndex,
						NodeModel.NodeGuid,
						FString::Printf(TEXT("Duplicate CachePoseName '%s'."), *NodeData->CachePoseName.ToString()));
					continue;
				}

				SeenNames.Add(NodeData->CachePoseName);
				OutCacheGraph.NamedCacheNames.Add(NodeData->CachePoseName);
			}

			return !HasErrors(OutMessages);
		}

		static bool CompileBodyPartGraph(
			const FLbbLayeredBlendBodyPartGraphModel& GraphModel,
			const int32 BodyPartIndex,
			const TSet<FName>& ValidNamedCaches,
			FLbbLayeredBlendBodyPart& OutBodyPart,
			TArray<FLbbCompileMessage>& OutMessages)
		{
			OutBodyPart.PartName = GraphModel.PartName;
#if WITH_EDITORONLY_DATA
			OutBodyPart.DebugColor = GraphModel.DebugColor;
#endif

			FGraphContext GraphContext(GraphModel, ELbbLayeredBlendBodyGraphKind::BodyPart);
			BuildGraphContext(GraphModel, GraphContext.GraphKind, BodyPartIndex, OutMessages, GraphContext);
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

			TMap<FGuid, FName> InternalCachePoseNames;
			TSet<FName> UsedInternalCachePoseNames;
			const FString InternalCacheScopeName = BuildInternalCacheScopeName(GraphContext.GraphKind, BodyPartIndex);
			int32 NextInternalCacheIndex = 0;
			for (const FGuid& NodeGuid : TopoAuthorNodes)
			{
				const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindDescriptor(GraphContext, NodeGuid);
				if (Descriptor == nullptr || !Descriptor->ProducesPose() || NodeGuid == FinalSourceGuid)
				{
					continue;
				}

				InternalCachePoseNames.Add(
					NodeGuid,
					AllocateInternalCachePoseName(
						InternalCacheScopeName,
						NextInternalCacheIndex++,
						ValidNamedCaches,
						UsedInternalCachePoseNames));
			}

			const FLbbLayeredBlendBodyGraphNodeCompileContext CompileContext(
				GraphContext.GraphKind,
				BodyPartIndex,
				[&GraphContext](const FGuid& NodeGuid, const FName PinName)
				{
					return FindInputLink(GraphContext, NodeGuid, PinName);
				},
				[&GraphContext, &InternalCachePoseNames, &ValidNamedCaches, &OutMessages, BodyPartIndex](const FGuid& SourceNodeGuid, FLbbLayeredBodyPartPoseSource& OutSourcePose)
				{
					return TryResolveCompiledSource(
						GraphContext,
						ELbbLayeredBlendBodyGraphKind::BodyPart,
						InternalCachePoseNames,
						ValidNamedCaches,
						BodyPartIndex,
						OutMessages,
						SourceNodeGuid,
						OutSourcePose);
				});

			for (const FGuid& NodeGuid : TopoAuthorNodes)
			{
				const FLbbLayeredBlendBodyGraphNodeModel* const* NodePtr = GraphContext.NodesByGuid.Find(NodeGuid);
				const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindDescriptor(GraphContext, NodeGuid);
				if (NodePtr == nullptr || Descriptor == nullptr)
				{
					continue;
				}

				if (!Descriptor->IsAuthorNode() || Descriptor->CompileNode == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, NodeGuid, TEXT("Unsupported author node type."));
					return false;
				}

				if (!Descriptor->ProducesPose())
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, NodeGuid, TEXT("BodyPart Graph author node must produce a pose."));
					return false;
				}

				const FLbbLayeredBodyPartPoseTarget TargetPose = (NodeGuid == FinalSourceGuid)
					? MakeCurrentPoseTarget()
					: MakeCachePoseTarget(InternalCachePoseNames.FindChecked(NodeGuid));

				if (!Descriptor->CompileNode(CompileContext, **NodePtr, TargetPose, OutBodyPart.Operators, OutMessages))
				{
					return false;
				}
			}

			const FLbbLayeredBlendBodyGraphNodeDescriptor* FinalDescriptor = FindDescriptor(GraphContext, FinalSourceGuid);
			if (FinalDescriptor == nullptr)
			{
				AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, BodyPartIndex, FinalSourceGuid, TEXT("Final source node descriptor is missing."));
				return false;
			}

			if (FinalDescriptor->bIsInputNode)
			{
				FLbbLayeredBodyPartPoseSource FinalSourcePose;
				if (!TryResolveCompiledSource(
						GraphContext,
						GraphContext.GraphKind,
						InternalCachePoseNames,
						ValidNamedCaches,
						BodyPartIndex,
						OutMessages,
						FinalSourceGuid,
						FinalSourcePose))
				{
					return false;
				}

				if (FinalSourcePose.Type != ELbbLayeredBodyPartPoseSourceType::CurrentPose)
				{
					FLbbLayeredBlendBodyPartOperator_CopyPose Operator;
					Operator.SourcePose = FinalSourcePose;
					Operator.Output = MakeCurrentPoseTarget();
					FInstancedStruct& OperatorData = OutBodyPart.Operators.AddDefaulted_GetRef();
					OperatorData.InitializeAs<FLbbLayeredBlendBodyPartOperator_CopyPose>();
					OperatorData.GetMutable<FLbbLayeredBlendBodyPartOperator_CopyPose>() = Operator;
				}
			}

			return !HasErrors(OutMessages);
		}

		static bool CompileCacheGraph(
			const FLbbLayeredBlendBodyCacheGraphModel& GraphModel,
			FCompiledCacheGraph& OutCacheGraph,
			TArray<FLbbCompileMessage>& OutMessages)
		{
			FGraphContext GraphContext(GraphModel, ELbbLayeredBlendBodyGraphKind::Cache);
			BuildGraphContext(GraphModel, GraphContext.GraphKind, INDEX_NONE, OutMessages, GraphContext);
			if (HasErrors(OutMessages))
			{
				return false;
			}

			CollectNamedCaches(GraphContext, INDEX_NONE, OutCacheGraph, OutMessages);
			if (HasErrors(OutMessages))
			{
				return false;
			}

			TSet<FName> ValidNamedCaches;
			for (const FName CacheName : OutCacheGraph.NamedCacheNames)
			{
				ValidNamedCaches.Add(CacheName);
			}
			TSet<FGuid> ReachableNodes;
			TArray<FGuid> TopoAuthorNodes;
			FGuid UnusedFinalSourceGuid;
			BuildReachableGraph(GraphContext, INDEX_NONE, ReachableNodes, TopoAuthorNodes, UnusedFinalSourceGuid, OutMessages);
			if (HasErrors(OutMessages))
			{
				return false;
			}

			TMap<FGuid, FName> InternalCachePoseNames;
			TSet<FName> UsedInternalCachePoseNames;
			const FString InternalCacheScopeName = BuildInternalCacheScopeName(GraphContext.GraphKind, INDEX_NONE);
			int32 NextInternalCacheIndex = 0;
			for (const FGuid& NodeGuid : TopoAuthorNodes)
			{
				const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindDescriptor(GraphContext, NodeGuid);
				if (Descriptor == nullptr || !Descriptor->ProducesPose())
				{
					continue;
				}

				InternalCachePoseNames.Add(
					NodeGuid,
					AllocateInternalCachePoseName(
						InternalCacheScopeName,
						NextInternalCacheIndex++,
						ValidNamedCaches,
						UsedInternalCachePoseNames));
			}

			const FLbbLayeredBlendBodyGraphNodeCompileContext CompileContext(
				GraphContext.GraphKind,
				INDEX_NONE,
				[&GraphContext](const FGuid& NodeGuid, const FName PinName)
				{
					return FindInputLink(GraphContext, NodeGuid, PinName);
				},
				[&GraphContext, &InternalCachePoseNames, &ValidNamedCaches, &OutMessages](const FGuid& SourceNodeGuid, FLbbLayeredBodyPartPoseSource& OutSourcePose)
				{
					return TryResolveCompiledSource(
						GraphContext,
						ELbbLayeredBlendBodyGraphKind::Cache,
						InternalCachePoseNames,
						ValidNamedCaches,
						INDEX_NONE,
						OutMessages,
						SourceNodeGuid,
						OutSourcePose);
				});

			for (const FGuid& NodeGuid : TopoAuthorNodes)
			{
				const FLbbLayeredBlendBodyGraphNodeModel* const* NodePtr = GraphContext.NodesByGuid.Find(NodeGuid);
				const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindDescriptor(GraphContext, NodeGuid);
				if (NodePtr == nullptr || Descriptor == nullptr)
				{
					continue;
				}

				if (!Descriptor->IsAuthorNode() || Descriptor->CompileNode == nullptr)
				{
					AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, INDEX_NONE, NodeGuid, TEXT("Unsupported author node type."));
					return false;
				}

				FLbbLayeredBodyPartPoseTarget TargetPose;
				if (Descriptor->bIsSavePoseNode)
				{
					const FLbbLayeredBlendBodyGraphNodeData_SavePose* NodeData = (*NodePtr)->NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_SavePose>();
					if (NodeData == nullptr)
					{
						AddMessage(OutMessages, EMessageSeverity::Error, GraphContext.GraphKind, INDEX_NONE, NodeGuid, TEXT("Save Pose node data is missing."));
						return false;
					}

					TargetPose = MakeCachePoseTarget(NodeData->CachePoseName);
				}
				else
				{
					TargetPose = MakeCachePoseTarget(InternalCachePoseNames.FindChecked(NodeGuid));
				}

				if (!Descriptor->CompileNode(CompileContext, **NodePtr, TargetPose, OutCacheGraph.Operators, OutMessages))
				{
					return false;
				}
			}

			return !HasErrors(OutMessages);
		}
	}

	void FLbbLayeredBlendBodyGraphCompiler::CreateDefaultCacheGraph(FLbbLayeredBlendBodyCacheGraphModel& OutGraphModel)
	{
		OutGraphModel = FLbbLayeredBlendBodyCacheGraphModel();
		OutGraphModel.GraphGuid = FGuid::NewGuid();
	}

	void FLbbLayeredBlendBodyGraphCompiler::CreateDefaultBodyPartGraph(
		FLbbLayeredBlendBodyPartGraphModel& OutGraphModel,
		const FName& PartName)
	{
		OutGraphModel = FLbbLayeredBlendBodyPartGraphModel();
		OutGraphModel.GraphGuid = FGuid::NewGuid();
		OutGraphModel.PartName = PartName;

		const FGuid InputGuid = AddNode<FLbbLayeredBlendBodyGraphNodeData_Input>(
			OutGraphModel,
			FVector2D(0.f, 120.f),
			[](FLbbLayeredBlendBodyGraphNodeData_Input& NodeData)
			{
				NodeData.SourcePose.Type = ELbbLayeredBodyPartPoseSourceType::CurrentPose;
			});
		const FGuid ResultGuid = AddNode<FLbbLayeredBlendBodyGraphNodeData_Result>(
			OutGraphModel,
			FVector2D(520.f, 120.f),
			[](FLbbLayeredBlendBodyGraphNodeData_Result&)
			{
			});

		AddLink(OutGraphModel, InputGuid, PinPose, ResultGuid, PinPose);
	}

FLbbCompileResult FLbbLayeredBlendBodyGraphCompiler::Compile(const ULbbLayeredBlendBodyDefinition& Definition)
	{
		FLbbCompileResult Result;
		Result.bSuccess = false;

#if !WITH_EDITORONLY_DATA
		Result.bSuccess = true;
		return Result;
#else
		FCompiledCacheGraph CompiledCacheGraph;
		CompileCacheGraph(Definition.EditorModel.CacheGraph, CompiledCacheGraph, Result.Messages);

		TSet<FName> ValidNamedCaches;
		for (const FName CacheName : CompiledCacheGraph.NamedCacheNames)
		{
			ValidNamedCaches.Add(CacheName);
		}
		TArray<FLbbLayeredBlendBodyPart> CompiledBodyParts;
		CompiledBodyParts.Reserve(Definition.EditorModel.BodyPartGraphs.Num());

		if (!HasErrors(Result.Messages))
		{
			for (int32 BodyPartIndex = 0; BodyPartIndex < Definition.EditorModel.BodyPartGraphs.Num(); ++BodyPartIndex)
			{
				const FLbbLayeredBlendBodyPartGraphModel& GraphModel = Definition.EditorModel.BodyPartGraphs[BodyPartIndex];
				FLbbLayeredBlendBodyPart& CompiledBodyPart = CompiledBodyParts.AddDefaulted_GetRef();
				CompileBodyPartGraph(GraphModel, BodyPartIndex, ValidNamedCaches, CompiledBodyPart, Result.Messages);
			}
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
					ELbbLayeredBlendBodyGraphKind::BodyPart,
					BodyPartIndex,
					FGuid(),
					FString::Printf(TEXT("BodyPart name '%s' is duplicated."), *PartName.ToString()));
			}
		}

		Result.bSuccess = !HasErrors(Result.Messages);

		if (Result.bSuccess)
		{
			Result.CompiledDefinition.CacheProgram.NamedCacheNames = MoveTemp(CompiledCacheGraph.NamedCacheNames);
			Result.CompiledDefinition.CacheProgram.Operators = MoveTemp(CompiledCacheGraph.Operators);
			Result.CompiledDefinition.BodyParts = MoveTemp(CompiledBodyParts);
		}

		return Result;
#endif
	}

void FLbbLayeredBlendBodyGraphCompiler::ApplyCompiledDefinition(
	const FLbbCompiledDefinitionData& CompiledDefinition,
	ULbbLayeredBlendBodyDefinition& Definition)
{
	Definition.CacheProgram = CompiledDefinition.CacheProgram;
	Definition.BodyParts = CompiledDefinition.BodyParts;
}
