// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbLayeredBlendBodyGraphNodeRegistry.h"

#include "LbbLayeredBlendBodyEdGraphNode.h"
#include "LbbLayeredBlendBodyGraphCompiler.h"
#include "LbbLayeredBlendBodyOperators.h"

namespace
{
	static const FName PinPose(TEXT("Pose"));
	static const FName PinInput(TEXT("Input"));
	static const FName PinBase(TEXT("Base"));
	static const FName PinBlend(TEXT("Blend"));
	static const FName PinAdditive(TEXT("Additive"));
	static const FName PinResult(TEXT("Result"));

	static const FName PoseInputPins[] = {PinPose};
	static const FName PoseOutputPins[] = {PinPose};
	static const FName InputPins[] = {PinInput};
	static const FName BaseInputPins[] = {PinBase};
	static const FName BaseBlendInputPins[] = {PinBase, PinBlend};
	static const FName BaseAdditiveInputPins[] = {PinBase, PinAdditive};
	static const FName ResultOutputPins[] = {PinResult};

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

	template <typename OperatorType>
	static void AddOperatorStruct(TArray<FInstancedStruct>& Operators, const OperatorType& Operator)
	{
		FInstancedStruct& OperatorData = Operators.AddDefaulted_GetRef();
		OperatorData.InitializeAs<OperatorType>();
		OperatorData.GetMutable<OperatorType>() = Operator;
	}

	static bool ResolveRequiredInputSource(
		const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
		const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
		const FName PinName,
		FLbbLayeredBodyPartPoseSource& OutSourcePose,
		TArray<FLbbCompileMessage>& OutMessages,
		const TCHAR* FailureMessage)
	{
		const FLbbLayeredBlendBodyGraphLinkModel* InputLink = CompileContext.FindInputLink(NodeModel.NodeGuid, PinName);
		if (InputLink == nullptr || !CompileContext.ResolveSource(InputLink->FromNodeGuid, OutSourcePose))
		{
			AddMessage(
				OutMessages,
				EMessageSeverity::Error,
				CompileContext.GetGraphKind(),
				CompileContext.GetBodyPartIndex(),
				NodeModel.NodeGuid,
				FailureMessage);
			return false;
		}

		return true;
	}

	static bool ValidateCurveWeight(
		const FLbbBlendWeight& Weight,
		const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
		const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
		const TCHAR* FailureMessage,
		TArray<FLbbCompileMessage>& OutMessages)
	{
		if (Weight.bUseBlendCurve && Weight.BlendCurveName.IsNone())
		{
			AddMessage(
				OutMessages,
				EMessageSeverity::Error,
				CompileContext.GetGraphKind(),
				CompileContext.GetBodyPartIndex(),
				NodeModel.NodeGuid,
				FailureMessage);
			return false;
		}

		return true;
	}

		static bool CompileApplySlotNode(
			const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
			const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
			const FLbbLayeredBodyPartPoseTarget& TargetPose,
			TArray<FInstancedStruct>& OutOperators,
			TArray<FLbbCompileMessage>& OutMessages)
		{
			FLbbLayeredBodyPartPoseSource SourcePose;
			if (!ResolveRequiredInputSource(
					CompileContext,
					NodeModel,
					PinInput,
					SourcePose,
					OutMessages,
					TEXT("Apply Slot failed to resolve its input.")))
			{
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeData_ApplySlot* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplySlot>();
			if (NodeData == nullptr || NodeData->SlotName.IsNone())
			{
				AddMessage(OutMessages, EMessageSeverity::Error, CompileContext.GetGraphKind(), CompileContext.GetBodyPartIndex(), NodeModel.NodeGuid, TEXT("Apply Slot requires a valid SlotName."));
				return false;
			}

			FLbbLayeredBlendBodyPartOperator_ApplySlot Operator;
			Operator.SourcePose = SourcePose;
			Operator.SlotName = NodeData->SlotName;
			Operator.Output = TargetPose;
			AddOperatorStruct(OutOperators, Operator);
			return true;
		}

		static bool CompileBlendNode(
			const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
			const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
			const FLbbLayeredBodyPartPoseTarget& TargetPose,
			TArray<FInstancedStruct>& OutOperators,
			TArray<FLbbCompileMessage>& OutMessages)
		{
			FLbbLayeredBodyPartPoseSource BaseSource;
			FLbbLayeredBodyPartPoseSource BlendSource;
			if (!ResolveRequiredInputSource(
					CompileContext,
					NodeModel,
					PinBase,
					BaseSource,
					OutMessages,
					TEXT("Blend failed to resolve one of its inputs."))
				|| !ResolveRequiredInputSource(
					CompileContext,
					NodeModel,
					PinBlend,
					BlendSource,
					OutMessages,
					TEXT("Blend failed to resolve one of its inputs.")))
			{
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeData_Blend* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_Blend>();
			if (NodeData == nullptr)
			{
				AddMessage(OutMessages, EMessageSeverity::Error, CompileContext.GetGraphKind(), CompileContext.GetBodyPartIndex(), NodeModel.NodeGuid, TEXT("Blend node data is missing."));
				return false;
			}

			if (!ValidateCurveWeight(NodeData->Weight, CompileContext, NodeModel, TEXT("Blend uses a curve weight but BlendCurveName is None."), OutMessages))
			{
				return false;
			}

			FLbbLayeredBlendBodyPartOperator_BlendTwoPoses Operator;
			Operator.BasePose = BaseSource;
			Operator.BlendPose = BlendSource;
			Operator.Weight = NodeData->Weight;
			Operator.Output = TargetPose;
			AddOperatorStruct(OutOperators, Operator);
			return true;
		}

		static bool CompileMaskedBlendNode(
			const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
			const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
			const FLbbLayeredBodyPartPoseTarget& TargetPose,
			TArray<FInstancedStruct>& OutOperators,
			TArray<FLbbCompileMessage>& OutMessages)
		{
			FLbbLayeredBodyPartPoseSource BaseSource;
			FLbbLayeredBodyPartPoseSource BlendSource;
			if (!ResolveRequiredInputSource(
					CompileContext,
					NodeModel,
					PinBase,
					BaseSource,
					OutMessages,
					TEXT("Masked Blend failed to resolve one of its inputs."))
				|| !ResolveRequiredInputSource(
					CompileContext,
					NodeModel,
					PinBlend,
					BlendSource,
					OutMessages,
					TEXT("Masked Blend failed to resolve one of its inputs.")))
			{
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeData_MaskedBlend* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_MaskedBlend>();
			if (NodeData == nullptr)
			{
				AddMessage(OutMessages, EMessageSeverity::Error, CompileContext.GetGraphKind(), CompileContext.GetBodyPartIndex(), NodeModel.NodeGuid, TEXT("Masked Blend node data is missing."));
				return false;
			}

			if (NodeData->BoneFilter.BranchFilters.IsEmpty())
			{
				AddMessage(OutMessages, EMessageSeverity::Error, CompileContext.GetGraphKind(), CompileContext.GetBodyPartIndex(), NodeModel.NodeGuid, TEXT("Masked Blend requires a non-empty BoneFilter."));
				return false;
			}

			if (!ValidateCurveWeight(NodeData->Weight, CompileContext, NodeModel, TEXT("Masked Blend uses a curve weight but BlendCurveName is None."), OutMessages))
			{
				return false;
			}

			FLbbLayeredBlendBodyPartOperator_MaskedBlend Operator;
			Operator.BasePose = BaseSource;
			Operator.BlendPose = BlendSource;
			Operator.BlendSpace = NodeData->BlendSpace;
			Operator.BoneFilter = NodeData->BoneFilter;
			Operator.Weight = NodeData->Weight;
			Operator.Output = TargetPose;
			AddOperatorStruct(OutOperators, Operator);
			return true;
		}

		static bool CompileSavePoseNode(
			const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
			const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
			const FLbbLayeredBodyPartPoseTarget& TargetPose,
			TArray<FInstancedStruct>& OutOperators,
			TArray<FLbbCompileMessage>& OutMessages)
		{
			FLbbLayeredBodyPartPoseSource InputPose;
			if (!ResolveRequiredInputSource(
					CompileContext,
					NodeModel,
					PinInput,
					InputPose,
					OutMessages,
					TEXT("Save Pose failed to resolve its input.")))
			{
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeData_SavePose* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_SavePose>();
			if (NodeData == nullptr || NodeData->CachePoseName.IsNone())
			{
				AddMessage(OutMessages, EMessageSeverity::Error, CompileContext.GetGraphKind(), CompileContext.GetBodyPartIndex(), NodeModel.NodeGuid, TEXT("Save Pose requires a valid CachePoseName."));
				return false;
			}

			FLbbLayeredBlendBodyPartOperator_CopyPose Operator;
			Operator.SourcePose = InputPose;
			Operator.Output = TargetPose;
			AddOperatorStruct(OutOperators, Operator);
			return true;
		}

		static bool CompileMakeAdditiveNode(
			const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
			const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
			const FLbbLayeredBodyPartPoseTarget& TargetPose,
			TArray<FInstancedStruct>& OutOperators,
			TArray<FLbbCompileMessage>& OutMessages)
		{
			FLbbLayeredBodyPartPoseSource BaseSource;
			FLbbLayeredBodyPartPoseSource AdditiveSource;
			if (!ResolveRequiredInputSource(
					CompileContext,
					NodeModel,
					PinBase,
					BaseSource,
					OutMessages,
					TEXT("Make Additive failed to resolve one of its inputs."))
				|| !ResolveRequiredInputSource(
					CompileContext,
					NodeModel,
					PinAdditive,
					AdditiveSource,
					OutMessages,
					TEXT("Make Additive failed to resolve one of its inputs.")))
			{
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeData_MakeAdditive* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_MakeAdditive>();
			if (NodeData == nullptr)
			{
				AddMessage(OutMessages, EMessageSeverity::Error, CompileContext.GetGraphKind(), CompileContext.GetBodyPartIndex(), NodeModel.NodeGuid, TEXT("Make Additive node data is missing."));
				return false;
			}

			FLbbLayeredBlendBodyPartOperator_MakeAdditive Operator;
			Operator.BasePose = BaseSource;
			Operator.AdditivePose = AdditiveSource;
			Operator.AdditiveSpace = NodeData->AdditiveSpace;
			Operator.Output = TargetPose;
			AddOperatorStruct(OutOperators, Operator);
			return true;
		}

		static bool CompileApplyAdditiveNode(
			const FLbbLayeredBlendBodyGraphNodeCompileContext& CompileContext,
			const FLbbLayeredBlendBodyGraphNodeModel& NodeModel,
			const FLbbLayeredBodyPartPoseTarget& TargetPose,
			TArray<FInstancedStruct>& OutOperators,
			TArray<FLbbCompileMessage>& OutMessages)
		{
			FLbbLayeredBodyPartPoseSource BaseSource;
			FLbbLayeredBodyPartPoseSource AdditiveSource;
			if (!ResolveRequiredInputSource(
					CompileContext,
					NodeModel,
					PinBase,
					BaseSource,
					OutMessages,
					TEXT("Apply Additive failed to resolve one of its inputs."))
				|| !ResolveRequiredInputSource(
					CompileContext,
					NodeModel,
					PinAdditive,
					AdditiveSource,
					OutMessages,
					TEXT("Apply Additive failed to resolve one of its inputs.")))
			{
				return false;
			}

			const FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive* NodeData = NodeModel.NodeData.GetPtr<FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive>();
			if (NodeData == nullptr)
			{
				AddMessage(OutMessages, EMessageSeverity::Error, CompileContext.GetGraphKind(), CompileContext.GetBodyPartIndex(), NodeModel.NodeGuid, TEXT("Apply Additive node data is missing."));
				return false;
			}

			if (!ValidateCurveWeight(NodeData->Weight, CompileContext, NodeModel, TEXT("Apply Additive uses a curve weight but BlendCurveName is None."), OutMessages))
			{
				return false;
			}

			FLbbLayeredBlendBodyPartOperator_ApplyAdditive Operator;
			Operator.BasePose = BaseSource;
			Operator.AdditivePose = AdditiveSource;
			Operator.AdditiveSpace = NodeData->AdditiveSpace;
			Operator.Weight = NodeData->Weight;
			Operator.Output = TargetPose;
			AddOperatorStruct(OutOperators, Operator);
			return true;
		}
	}

	FLbbLayeredBlendBodyGraphNodeCompileContext::FLbbLayeredBlendBodyGraphNodeCompileContext(
		const ELbbLayeredBlendBodyGraphKind InGraphKind,
		const int32 InBodyPartIndex,
		FFindInputLinkCallback InFindInputLinkCallback,
		FResolveSourceCallback InResolveSourceCallback)
		: GraphKind(InGraphKind)
		, BodyPartIndex(InBodyPartIndex)
		, FindInputLinkCallback(MoveTemp(InFindInputLinkCallback))
		, ResolveSourceCallback(MoveTemp(InResolveSourceCallback))
	{
	}

	const FLbbLayeredBlendBodyGraphLinkModel* FLbbLayeredBlendBodyGraphNodeCompileContext::FindInputLink(const FGuid& NodeGuid, const FName PinName) const
	{
		return FindInputLinkCallback ? FindInputLinkCallback(NodeGuid, PinName) : nullptr;
	}

	bool FLbbLayeredBlendBodyGraphNodeCompileContext::ResolveSource(const FGuid& SourceNodeGuid, FLbbLayeredBodyPartPoseSource& OutSourcePose) const
	{
		return ResolveSourceCallback ? ResolveSourceCallback(SourceNodeGuid, OutSourcePose) : false;
	}

	bool FLbbLayeredBlendBodyGraphNodeDescriptor::IsAllowedInGraph(const ELbbLayeredBlendBodyGraphKind GraphKind) const
	{
		const ELbbLayeredBlendBodyGraphKindFlags KindFlag =
			(GraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
				? ELbbLayeredBlendBodyGraphKindFlags::Cache
				: ELbbLayeredBlendBodyGraphKindFlags::BodyPart;
		return EnumHasAnyFlags(AllowedGraphKinds, KindFlag);
	}

	bool FLbbLayeredBlendBodyGraphNodeDescriptor::IsSingletonInGraph(const ELbbLayeredBlendBodyGraphKind GraphKind) const
	{
		const ELbbLayeredBlendBodyGraphKindFlags KindFlag =
			(GraphKind == ELbbLayeredBlendBodyGraphKind::Cache)
				? ELbbLayeredBlendBodyGraphKindFlags::Cache
				: ELbbLayeredBlendBodyGraphKindFlags::BodyPart;
		return bIsSingleton && EnumHasAnyFlags(SingletonGraphKinds, KindFlag);
	}

const TArray<FLbbLayeredBlendBodyGraphNodeDescriptor>& GetLbbLayeredBlendBodyGraphNodeDescriptors()
	{
		static const TArray<FLbbLayeredBlendBodyGraphNodeDescriptor> Descriptors = {
			{
				FLbbLayeredBlendBodyGraphNodeData_Input::StaticStruct(),
				ULbbLayeredBlendBodyEdGraphNode_Input::StaticClass(),
				TEXT("Input"),
				nullptr,
				0,
				PoseOutputPins,
				UE_ARRAY_COUNT(PoseOutputPins),
				true,
				false,
				false,
				true,
				false,
				ELbbLayeredBlendBodyGraphKindFlags::None,
				ELbbLayeredBlendBodyGraphKindFlags::All,
				nullptr
			},
			{
				FLbbLayeredBlendBodyGraphNodeData_Result::StaticStruct(),
				ULbbLayeredBlendBodyEdGraphNode_Result::StaticClass(),
				TEXT("Result"),
				PoseInputPins,
				UE_ARRAY_COUNT(PoseInputPins),
				nullptr,
				0,
				false,
				true,
				false,
				false,
				true,
				ELbbLayeredBlendBodyGraphKindFlags::BodyPart,
				ELbbLayeredBlendBodyGraphKindFlags::BodyPart,
				nullptr
			},
			{
				FLbbLayeredBlendBodyGraphNodeData_ApplySlot::StaticStruct(),
				ULbbLayeredBlendBodyEdGraphNode_ApplySlot::StaticClass(),
				TEXT("Apply Slot"),
				InputPins,
				UE_ARRAY_COUNT(InputPins),
				ResultOutputPins,
				UE_ARRAY_COUNT(ResultOutputPins),
				false,
				false,
				false,
				true,
				false,
				ELbbLayeredBlendBodyGraphKindFlags::None,
				ELbbLayeredBlendBodyGraphKindFlags::BodyPart,
				&CompileApplySlotNode
			},
			{
				FLbbLayeredBlendBodyGraphNodeData_Blend::StaticStruct(),
				ULbbLayeredBlendBodyEdGraphNode_Blend::StaticClass(),
				TEXT("Blend"),
				BaseBlendInputPins,
				UE_ARRAY_COUNT(BaseBlendInputPins),
				ResultOutputPins,
				UE_ARRAY_COUNT(ResultOutputPins),
				false,
				false,
				false,
				true,
				false,
				ELbbLayeredBlendBodyGraphKindFlags::None,
				ELbbLayeredBlendBodyGraphKindFlags::All,
				&CompileBlendNode
			},
			{
				FLbbLayeredBlendBodyGraphNodeData_MaskedBlend::StaticStruct(),
				ULbbLayeredBlendBodyEdGraphNode_MaskedBlend::StaticClass(),
				TEXT("Masked Blend"),
				BaseBlendInputPins,
				UE_ARRAY_COUNT(BaseBlendInputPins),
				ResultOutputPins,
				UE_ARRAY_COUNT(ResultOutputPins),
				false,
				false,
				false,
				true,
				false,
				ELbbLayeredBlendBodyGraphKindFlags::None,
				ELbbLayeredBlendBodyGraphKindFlags::All,
				&CompileMaskedBlendNode
			},
			{
				FLbbLayeredBlendBodyGraphNodeData_SavePose::StaticStruct(),
				ULbbLayeredBlendBodyEdGraphNode_SavePose::StaticClass(),
				TEXT("Save Pose"),
				InputPins,
				UE_ARRAY_COUNT(InputPins),
				nullptr,
				0,
				false,
				false,
				true,
				true,
				false,
				ELbbLayeredBlendBodyGraphKindFlags::Cache,
				ELbbLayeredBlendBodyGraphKindFlags::Cache,
				&CompileSavePoseNode
			},
			{
				FLbbLayeredBlendBodyGraphNodeData_MakeAdditive::StaticStruct(),
				ULbbLayeredBlendBodyEdGraphNode_MakeAdditive::StaticClass(),
				TEXT("Make Additive"),
				BaseAdditiveInputPins,
				UE_ARRAY_COUNT(BaseAdditiveInputPins),
				ResultOutputPins,
				UE_ARRAY_COUNT(ResultOutputPins),
				false,
				false,
				false,
				true,
				false,
				ELbbLayeredBlendBodyGraphKindFlags::Cache,
				ELbbLayeredBlendBodyGraphKindFlags::Cache,
				&CompileMakeAdditiveNode
			},
			{
				FLbbLayeredBlendBodyGraphNodeData_ApplyAdditive::StaticStruct(),
				ULbbLayeredBlendBodyEdGraphNode_ApplyAdditive::StaticClass(),
				TEXT("Apply Additive"),
				BaseAdditiveInputPins,
				UE_ARRAY_COUNT(BaseAdditiveInputPins),
				ResultOutputPins,
				UE_ARRAY_COUNT(ResultOutputPins),
				false,
				false,
				false,
				true,
				false,
				ELbbLayeredBlendBodyGraphKindFlags::None,
				ELbbLayeredBlendBodyGraphKindFlags::All,
				&CompileApplyAdditiveNode
			}
		};

		return Descriptors;
	}

const FLbbLayeredBlendBodyGraphNodeDescriptor* FindLbbLayeredBlendBodyGraphNodeDescriptor(const UScriptStruct* NodeDataStruct)
	{
		if (NodeDataStruct == nullptr)
		{
			return nullptr;
		}

		for (const FLbbLayeredBlendBodyGraphNodeDescriptor& Descriptor : GetLbbLayeredBlendBodyGraphNodeDescriptors())
		{
			if (Descriptor.NodeDataStruct == NodeDataStruct)
			{
				return &Descriptor;
			}
		}

		return nullptr;
	}

ULbbLayeredBlendBodyEdGraphNode* CreateLbbLayeredBlendBodyNodeForDataStruct(UEdGraph* OuterGraph, const UScriptStruct* NodeDataStruct)
	{
		const FLbbLayeredBlendBodyGraphNodeDescriptor* Descriptor = FindLbbLayeredBlendBodyGraphNodeDescriptor(NodeDataStruct);
		if (Descriptor == nullptr || Descriptor->NodeClass == nullptr || OuterGraph == nullptr)
		{
			return nullptr;
		}

		return NewObject<ULbbLayeredBlendBodyEdGraphNode>(OuterGraph, Descriptor->NodeClass, NAME_None, RF_Transactional);
	}
