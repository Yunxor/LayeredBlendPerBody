// Fill out your copyright notice in the Description page of Project Settings.

#include "LbbOperators.h"

#include "LbbExecutionContext.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"

namespace
{
	static FString GetPoseSourceLabel(const FLbbLayeredBodyPartPoseSource& Source)
	{
		switch (Source.Type)
		{
		case ELbbLayeredBodyPartPoseSourceType::Motion:
			return TEXT("Motion");
		case ELbbLayeredBodyPartPoseSourceType::BasePose:
			return TEXT("Base Pose");
		case ELbbLayeredBodyPartPoseSourceType::OverlayPose:
			return TEXT("Overlay Pose");
		case ELbbLayeredBodyPartPoseSourceType::CurrentPose:
			return TEXT("Current Pose");
		case ELbbLayeredBodyPartPoseSourceType::CachePose:
			return FString::Printf(TEXT("Cache(%s)"), Source.CachePoseName.IsNone() ? TEXT("<None>") : *Source.CachePoseName.ToString());
		case ELbbLayeredBodyPartPoseSourceType::InputPose:
			return FString::Printf(TEXT("Input(%s)"), Source.InputPoseName.IsNone() ? TEXT("<None>") : *Source.InputPoseName.ToString());
		default:
			return TEXT("Unknown");
		}
	}

	static FString GetPoseTargetLabel(const FLbbLayeredBodyPartPoseTarget& Target)
	{
		switch (Target.Type)
		{
		case ELbbLayeredBodyPartPoseTargetType::CurrentPose:
			return TEXT("Current Pose");
		case ELbbLayeredBodyPartPoseTargetType::CachePose:
			return FString::Printf(TEXT("Cache(%s)"), Target.CachePoseName.IsNone() ? TEXT("<None>") : *Target.CachePoseName.ToString());
		default:
			return TEXT("Unknown");
		}
	}

	static FString GetBoneSpaceLabel(const ELbbBoneSpace BoneSpace)
	{
		return BoneSpace == ELbbBoneSpace::MeshSpace
			? TEXT("Mesh Space")
			: TEXT("Local Space");
	}

	static FString GetBlendWeightLabel(const FLbbBlendWeight& Weight)
	{
		if (Weight.bUseBlendCurve)
		{
			return FString::Printf(
				TEXT("Curve(%s)"),
				Weight.BlendCurveName.IsNone() ? TEXT("<None>") : *Weight.BlendCurveName.ToString());
		}

		return FString::Printf(TEXT("%.3f"), Weight.BlendWeight);
	}

	static void UpdateCurveWeight(FLbbBlendWeight& BlendWeight, const FAnimInstanceProxy* InstanceProxy)
	{
		if (InstanceProxy == nullptr || !BlendWeight.bUseBlendCurve)
		{
			return;
		}

		const UAnimInstance* AnimInstance = Cast<UAnimInstance>(InstanceProxy->GetAnimInstanceObject());
		if (AnimInstance == nullptr)
		{
			return;
		}

		BlendWeight.BlendWeight = BlendWeight.ScaleBias.ApplyTo(AnimInstance->GetCurveValue(BlendWeight.BlendCurveName));
	}

	class FWeightedCompiledOperator : public FLbbCompiledOperator
	{
	public:
		explicit FWeightedCompiledOperator(const FLbbBlendWeight& InWeight)
			: Weight(InWeight)
		{
		}

		virtual void UpdateBlendWeight(const FAnimInstanceProxy* InstanceProxy) override
		{
			UpdateCurveWeight(Weight, InstanceProxy);
		}

	protected:
		FLbbBlendWeight Weight;
	};

	class FCopyPoseCompiledOperator final : public FLbbCompiledOperator
	{
	public:
		FCopyPoseCompiledOperator(
			const FLbbCompiledPoseSource& InSourcePose,
			const FLbbCompiledPoseTarget& InTargetPose)
			: SourcePose(InSourcePose)
			, TargetPose(InTargetPose)
		{
		}

		virtual void AccumulateRequirements(FLbbOperatorRequirements& Requirements) const override
		{
			AccumulateCommonRequirements(Requirements, &SourcePose, nullptr, &TargetPose);
		}

		virtual void Execute(FLbbOperatorExecutionContext& ExecutionContext) const override
		{
			FLbbOperatorExecutionContext::AssignTarget(
				ExecutionContext,
				TargetPose,
				FLbbOperatorExecutionContext::ResolveSource(ExecutionContext, SourcePose));
		}

	private:
		FLbbCompiledPoseSource SourcePose;
		FLbbCompiledPoseTarget TargetPose;
	};

	class FApplySlotCompiledOperator final : public FLbbCompiledOperator
	{
	public:
		FApplySlotCompiledOperator(
			const FLbbCompiledPoseSource& InSourcePose,
			const FLbbCompiledPoseTarget& InTargetPose,
			const int32 InSlotNodeIndex)
			: SourcePose(InSourcePose)
			, TargetPose(InTargetPose)
			, SlotNodeIndex(InSlotNodeIndex)
		{
		}

		virtual void AccumulateRequirements(FLbbOperatorRequirements& Requirements) const override
		{
			AccumulateCommonRequirements(Requirements, &SourcePose, nullptr, &TargetPose);
			Requirements.bNeedsSlotEvaluation = true;
		}

		virtual void Execute(FLbbOperatorExecutionContext& ExecutionContext) const override
		{
			const FPoseContext& Source = FLbbOperatorExecutionContext::ResolveSource(ExecutionContext, SourcePose);
			FPoseContext& Target = FLbbOperatorExecutionContext::ResolveTarget(ExecutionContext, TargetPose);
			if (!FLbbOperatorExecutionContext::CanEvaluateSlot(ExecutionContext, SlotNodeIndex))
			{
				Target = Source;
				return;
			}

			FLbbOperatorExecutionContext::EvaluateSlot(ExecutionContext, SlotNodeIndex, Source, Target);
		}

	private:
		FLbbCompiledPoseSource SourcePose;
		FLbbCompiledPoseTarget TargetPose;
		int32 SlotNodeIndex = INDEX_NONE;
	};

	class FApplyAdditiveCompiledOperator final : public FWeightedCompiledOperator
	{
	public:
		FApplyAdditiveCompiledOperator(
			const FLbbCompiledPoseSource& InBasePose,
			const FLbbCompiledPoseSource& InAdditivePose,
			const FLbbCompiledPoseTarget& InTargetPose,
			const FLbbBlendWeight& InWeight,
			const bool bInMeshSpace)
			: FWeightedCompiledOperator(InWeight)
			, BasePose(InBasePose)
			, AdditivePose(InAdditivePose)
			, TargetPose(InTargetPose)
			, bMeshSpace(bInMeshSpace)
		{
		}

		virtual void AccumulateRequirements(FLbbOperatorRequirements& Requirements) const override
		{
			AccumulateCommonRequirements(Requirements, &BasePose, &AdditivePose, &TargetPose);
		}

		virtual void Execute(FLbbOperatorExecutionContext& ExecutionContext) const override
		{
			const FPoseContext& Base = FLbbOperatorExecutionContext::ResolveSource(ExecutionContext, BasePose);
			const FPoseContext& Additive = FLbbOperatorExecutionContext::ResolveSource(ExecutionContext, AdditivePose);
			FPoseContext& Target = FLbbOperatorExecutionContext::ResolveTarget(ExecutionContext, TargetPose);
			Target = Base;

			const float BlendWeight = Weight.GetBlendWeight();
			if (FAnimWeight::IsRelevant(BlendWeight))
			{
				FLbbOperatorExecutionContext::AccumulateAdditive(ExecutionContext, Target, Additive, bMeshSpace, BlendWeight);
			}
		}

	private:
		FLbbCompiledPoseSource BasePose;
		FLbbCompiledPoseSource AdditivePose;
		FLbbCompiledPoseTarget TargetPose;
		bool bMeshSpace = false;
	};

	class FMakeAdditiveCompiledOperator final : public FLbbCompiledOperator
	{
	public:
		FMakeAdditiveCompiledOperator(
			const FLbbCompiledPoseSource& InBasePose,
			const FLbbCompiledPoseSource& InAdditivePose,
			const FLbbCompiledPoseTarget& InTargetPose,
			const bool bInMeshSpace)
			: BasePose(InBasePose)
			, AdditivePose(InAdditivePose)
			, TargetPose(InTargetPose)
			, bMeshSpace(bInMeshSpace)
		{
		}

		virtual void AccumulateRequirements(FLbbOperatorRequirements& Requirements) const override
		{
			AccumulateCommonRequirements(Requirements, &BasePose, &AdditivePose, &TargetPose);
		}

		virtual void Execute(FLbbOperatorExecutionContext& ExecutionContext) const override
		{
			const FPoseContext& Base = FLbbOperatorExecutionContext::ResolveSource(ExecutionContext, BasePose);
			const FPoseContext& Additive = FLbbOperatorExecutionContext::ResolveSource(ExecutionContext, AdditivePose);
			FPoseContext& Target = FLbbOperatorExecutionContext::ResolveTarget(ExecutionContext, TargetPose);
			FLbbOperatorExecutionContext::MakeAdditive(ExecutionContext, Base, Additive, Target, bMeshSpace);
		}

	private:
		FLbbCompiledPoseSource BasePose;
		FLbbCompiledPoseSource AdditivePose;
		FLbbCompiledPoseTarget TargetPose;
		bool bMeshSpace = false;
	};

	class FBlendTwoPosesCompiledOperator final : public FWeightedCompiledOperator
	{
	public:
		FBlendTwoPosesCompiledOperator(
			const FLbbCompiledPoseSource& InBasePose,
			const FLbbCompiledPoseSource& InBlendPose,
			const FLbbCompiledPoseTarget& InTargetPose,
			const FLbbBlendWeight& InWeight)
			: FWeightedCompiledOperator(InWeight)
			, BasePose(InBasePose)
			, BlendPose(InBlendPose)
			, TargetPose(InTargetPose)
		{
		}

		virtual void AccumulateRequirements(FLbbOperatorRequirements& Requirements) const override
		{
			AccumulateCommonRequirements(Requirements, &BasePose, &BlendPose, &TargetPose);
		}

		virtual void Execute(FLbbOperatorExecutionContext& ExecutionContext) const override
		{
			const FPoseContext& Base = FLbbOperatorExecutionContext::ResolveSource(ExecutionContext, BasePose);
			const FPoseContext& Blend = FLbbOperatorExecutionContext::ResolveSource(ExecutionContext, BlendPose);
			FPoseContext& Target = FLbbOperatorExecutionContext::ResolveTarget(ExecutionContext, TargetPose);

			const float BlendWeight = Weight.GetBlendWeight();
			if (!FAnimWeight::IsRelevant(BlendWeight))
			{
				Target = Base;
				return;
			}

			if (FAnimWeight::IsFullWeight(BlendWeight))
			{
				Target = Blend;
				return;
			}

			FLbbOperatorExecutionContext::BlendTwoPoses(ExecutionContext, Base, Blend, Target, BlendWeight);
		}

	private:
		FLbbCompiledPoseSource BasePose;
		FLbbCompiledPoseSource BlendPose;
		FLbbCompiledPoseTarget TargetPose;
	};

	class FMaskedBlendCompiledOperator final : public FWeightedCompiledOperator
	{
	public:
		FMaskedBlendCompiledOperator(
			const FLbbCompiledPoseSource& InBasePose,
			const FLbbCompiledPoseSource& InBlendPose,
			const FLbbCompiledPoseTarget& InTargetPose,
			const FInputBlendPose& InBoneFilter,
			const FLbbBlendWeight& InWeight,
			const bool bInMeshSpace)
			: FWeightedCompiledOperator(InWeight)
			, BasePose(InBasePose)
			, BlendPose(InBlendPose)
			, TargetPose(InTargetPose)
			, BoneFilter(InBoneFilter)
			, bMeshSpace(bInMeshSpace)
		{
		}

		virtual void BuildBoneBlendWeights(const USkeleton* InSkeleton) override;
		virtual void RebuildBoneBlendWeights(const FBoneContainer& RequiredBones) override;
		virtual void AccumulateRequirements(FLbbOperatorRequirements& Requirements) const override;
		virtual void Execute(FLbbOperatorExecutionContext& ExecutionContext) const override;

	private:
		void UpdateCurrentBlendWeights(float BlendWeight) const;

		FLbbCompiledPoseSource BasePose;
		FLbbCompiledPoseSource BlendPose;
		FLbbCompiledPoseTarget TargetPose;
		FInputBlendPose BoneFilter;
		mutable TArray<FPerBoneBlendWeight> SkeletonBoneWeights;
		mutable TArray<FPerBoneBlendWeight> DesiredBoneWeights;
		mutable TArray<FPerBoneBlendWeight> CurrentBoneWeights;
		bool bMeshSpace = false;
	};
}

void FLbbCompiledOperator::BuildBoneBlendWeights(const USkeleton* InSkeleton)
{
}

void FLbbCompiledOperator::RebuildBoneBlendWeights(const FBoneContainer& RequiredBones)
{
}

void FLbbCompiledOperator::UpdateBlendWeight(const FAnimInstanceProxy* InstanceProxy)
{
}

void FLbbCompiledOperator::AccumulateCommonRequirements(
	FLbbOperatorRequirements& Requirements,
	const FLbbCompiledPoseSource* SourceA,
	const FLbbCompiledPoseSource* SourceB,
	const FLbbCompiledPoseTarget* Target)
{
	auto MarkSourceUsage = [&Requirements](const FLbbCompiledPoseSource* Source)
	{
		if (Source == nullptr)
		{
			return;
		}

		switch (Source->Kind)
		{
		case ELbbCompiledPoseSourceKind::BasePose:
			Requirements.bNeedsBasePose = true;
			break;
		case ELbbCompiledPoseSourceKind::OverlayPose:
			Requirements.bNeedsOverlayPose = true;
			break;
		case ELbbCompiledPoseSourceKind::InputPose:
			if (!Source->PoseName.IsNone())
			{
				Requirements.UsedInputPoseNames.Add(Source->PoseName);
			}
			break;
		default:
			break;
		}
	};

	MarkSourceUsage(SourceA);
	MarkSourceUsage(SourceB);

	if (Target != nullptr && Target->Kind == ELbbCompiledPoseTargetKind::CurrentPose)
	{
		Requirements.bCanAffectCurrentPose = true;
	}
}

void FMaskedBlendCompiledOperator::BuildBoneBlendWeights(const USkeleton* InSkeleton)
{
	SkeletonBoneWeights.Reset();
	DesiredBoneWeights.Reset();
	CurrentBoneWeights.Reset();

	if (InSkeleton == nullptr)
	{
		return;
	}

	TArray<FInputBlendPose> BlendFilters;
	BlendFilters.Add(BoneFilter);
	FAnimationRuntime::CreateMaskWeights(SkeletonBoneWeights, BlendFilters, InSkeleton);
}

void FMaskedBlendCompiledOperator::RebuildBoneBlendWeights(const FBoneContainer& RequiredBones)
{
	const int32 NumRequiredBones = RequiredBones.GetBoneIndicesArray().Num();
	DesiredBoneWeights.SetNumZeroed(NumRequiredBones);
	for (int32 RequiredBoneIndex = 0; RequiredBoneIndex < NumRequiredBones; ++RequiredBoneIndex)
	{
		const int32 SkeletonBoneIndex = RequiredBones.GetSkeletonIndex(FCompactPoseBoneIndex(RequiredBoneIndex));
		if (SkeletonBoneWeights.IsValidIndex(SkeletonBoneIndex))
		{
			DesiredBoneWeights[RequiredBoneIndex] = SkeletonBoneWeights[SkeletonBoneIndex];
		}
	}

	CurrentBoneWeights.SetNumZeroed(NumRequiredBones);
}

void FMaskedBlendCompiledOperator::AccumulateRequirements(FLbbOperatorRequirements& Requirements) const
{
	AccumulateCommonRequirements(Requirements, &BasePose, &BlendPose, &TargetPose);
}

void FMaskedBlendCompiledOperator::Execute(FLbbOperatorExecutionContext& ExecutionContext) const
{
	const FPoseContext& Base = FLbbOperatorExecutionContext::ResolveSource(ExecutionContext, BasePose);
	const FPoseContext& Blend = FLbbOperatorExecutionContext::ResolveSource(ExecutionContext, BlendPose);
	FPoseContext& Target = FLbbOperatorExecutionContext::ResolveTarget(ExecutionContext, TargetPose);

	const float BlendWeight = FMath::Clamp(Weight.GetBlendWeight(), 0.f, 1.f);
	if (!FAnimWeight::IsRelevant(BlendWeight))
	{
		Target = Base;
		return;
	}

	if (DesiredBoneWeights.IsEmpty())
	{
		Target = Base;
		return;
	}

	FPoseContext BasePoseCopy(ExecutionContext.CurrentPose);
	BasePoseCopy = Base;
	FPoseContext BlendPoseCopy(ExecutionContext.CurrentPose);
	BlendPoseCopy = Blend;
	Target = Base;

	TArray<FCompactPose, TInlineAllocator<1>> BlendPoses;
	BlendPoses.Add(BlendPoseCopy.Pose);

	TArray<FBlendedCurve, TInlineAllocator<1>> BlendCurves;
	BlendCurves.Add(BlendPoseCopy.Curve);

	TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<1>> BlendAttributes;
	BlendAttributes.Add(BlendPoseCopy.CustomAttributes);

	UpdateCurrentBlendWeights(BlendWeight);

	const FAnimationRuntime::EBlendPosesPerBoneFilterFlags BlendFlags = bMeshSpace
		? FAnimationRuntime::EBlendPosesPerBoneFilterFlags::MeshSpaceRotation
		: FAnimationRuntime::EBlendPosesPerBoneFilterFlags::None;

	FAnimationPoseData OutputPoseData(Target);
	FAnimationRuntime::BlendPosesPerBoneFilter(
		BasePoseCopy.Pose,
		BlendPoses,
		BasePoseCopy.Curve,
		BlendCurves,
		BasePoseCopy.CustomAttributes,
		BlendAttributes,
		OutputPoseData,
		CurrentBoneWeights,
		BlendFlags,
		ECurveBlendOption::Type::DoNotOverride);
}

void FMaskedBlendCompiledOperator::UpdateCurrentBlendWeights(const float BlendWeight) const
{
	if (CurrentBoneWeights.Num() != DesiredBoneWeights.Num())
	{
		CurrentBoneWeights.SetNumZeroed(DesiredBoneWeights.Num());
	}

	if (CurrentBoneWeights.IsEmpty())
	{
		return;
	}

	const float ClampedBlendWeight = FMath::Clamp(BlendWeight, 0.f, 1.f);
	if (!FAnimWeight::IsRelevant(ClampedBlendWeight))
	{
		FMemory::Memzero(CurrentBoneWeights.GetData(), CurrentBoneWeights.Num() * sizeof(FPerBoneBlendWeight));
		return;
	}

	float BlendWeights[1] = {ClampedBlendWeight};
	FAnimationRuntime::UpdateDesiredBoneWeight(DesiredBoneWeights, CurrentBoneWeights, MakeArrayView(BlendWeights, 1));
}

FString FLbbPartOperatorBase::ToString() const
{
	return TEXT("<Invalid Operator>");
}

TUniquePtr<FLbbCompiledOperator> FLbbPartOperatorBase::CreateCompiledOperator(
	ILbbOperatorCompileContext& CompileContext) const
{
	ensureMsgf(false, TEXT("FLbbPartOperatorBase::CreateCompiledOperator should be overridden by derived operators."));
	return nullptr;
}

FString FLbbPartOperator_CopyPose::ToString() const
{
	return FString::Printf(
		TEXT("Copy Pose | Source: %s | Output: %s"),
		*GetPoseSourceLabel(SourcePose),
		*GetPoseTargetLabel(Output));
}

TUniquePtr<FLbbCompiledOperator> FLbbPartOperator_CopyPose::CreateCompiledOperator(
	ILbbOperatorCompileContext& CompileContext) const
{
	return MakeUnique<FCopyPoseCompiledOperator>(
		CompileContext.CompileSource(SourcePose),
		CompileContext.CompileTarget(Output));
}

FString FLbbPartOperator_ApplySlot::ToString() const
{
	return FString::Printf(
		TEXT("Slot | Input: %s | Slot: %s | Output: %s"),
		*GetPoseSourceLabel(SourcePose),
		SlotName.IsNone() ? TEXT("<None>") : *SlotName.ToString(),
		*GetPoseTargetLabel(Output));
}

TUniquePtr<FLbbCompiledOperator> FLbbPartOperator_ApplySlot::CreateCompiledOperator(
	ILbbOperatorCompileContext& CompileContext) const
{
	return MakeUnique<FApplySlotCompiledOperator>(
		CompileContext.CompileSource(SourcePose),
		CompileContext.CompileTarget(Output),
		CompileContext.FindOrAddSlotNode(SlotName));
}

FString FLbbPartOperator_ApplyAdditive::ToString() const
{
	return FString::Printf(
		TEXT("Apply Additive | Base: %s | Additive: %s | Space: %s | Weight: %s | Output: %s"),
		*GetPoseSourceLabel(BasePose),
		*GetPoseSourceLabel(AdditivePose),
		*GetBoneSpaceLabel(AdditiveSpace),
		*GetBlendWeightLabel(Weight),
		*GetPoseTargetLabel(Output));
}

TUniquePtr<FLbbCompiledOperator> FLbbPartOperator_ApplyAdditive::CreateCompiledOperator(
	ILbbOperatorCompileContext& CompileContext) const
{
	return MakeUnique<FApplyAdditiveCompiledOperator>(
		CompileContext.CompileSource(BasePose),
		CompileContext.CompileSource(AdditivePose),
		CompileContext.CompileTarget(Output),
		Weight,
		AdditiveSpace == ELbbBoneSpace::MeshSpace);
}

FString FLbbPartOperator_MakeAdditive::ToString() const
{
	return FString::Printf(
		TEXT("Make Additive | Base: %s | Additive: %s | Space: %s | Output: %s"),
		*GetPoseSourceLabel(BasePose),
		*GetPoseSourceLabel(AdditivePose),
		*GetBoneSpaceLabel(AdditiveSpace),
		*GetPoseTargetLabel(Output));
}

TUniquePtr<FLbbCompiledOperator> FLbbPartOperator_MakeAdditive::CreateCompiledOperator(
	ILbbOperatorCompileContext& CompileContext) const
{
	return MakeUnique<FMakeAdditiveCompiledOperator>(
		CompileContext.CompileSource(BasePose),
		CompileContext.CompileSource(AdditivePose),
		CompileContext.CompileTarget(Output),
		AdditiveSpace == ELbbBoneSpace::MeshSpace);
}

FString FLbbPartOperator_BlendTwoPoses::ToString() const
{
	return FString::Printf(
		TEXT("Blend Two Poses | Base: %s | Blend: %s | Weight: %s | Output: %s"),
		*GetPoseSourceLabel(BasePose),
		*GetPoseSourceLabel(BlendPose),
		*GetBlendWeightLabel(Weight),
		*GetPoseTargetLabel(Output));
}

TUniquePtr<FLbbCompiledOperator> FLbbPartOperator_BlendTwoPoses::CreateCompiledOperator(
	ILbbOperatorCompileContext& CompileContext) const
{
	return MakeUnique<FBlendTwoPosesCompiledOperator>(
		CompileContext.CompileSource(BasePose),
		CompileContext.CompileSource(BlendPose),
		CompileContext.CompileTarget(Output),
		Weight);
}

FString FLbbPartOperator_MaskedBlend::ToString() const
{
	return FString::Printf(
		TEXT("Layered Blend Per Bone | Base: %s | Blend: %s | Space: %s | Filters: %d | Weight: %s | Output: %s"),
		*GetPoseSourceLabel(BasePose),
		*GetPoseSourceLabel(BlendPose),
		*GetBoneSpaceLabel(BlendSpace),
		BoneFilter.BranchFilters.Num(),
		*GetBlendWeightLabel(Weight),
		*GetPoseTargetLabel(Output));
}

TUniquePtr<FLbbCompiledOperator> FLbbPartOperator_MaskedBlend::CreateCompiledOperator(
	ILbbOperatorCompileContext& CompileContext) const
{
	return MakeUnique<FMaskedBlendCompiledOperator>(
		CompileContext.CompileSource(BasePose),
		CompileContext.CompileSource(BlendPose),
		CompileContext.CompileTarget(Output),
		BoneFilter,
		Weight,
		BlendSpace == ELbbBoneSpace::MeshSpace);
}
