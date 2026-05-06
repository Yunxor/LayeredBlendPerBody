// Fill out your copyright notice in the Description page of Project Settings.


#include "LbbLayeredBlendBodyDefinition.h"
#include "LbbLayeredBlendBodyExecutionContext.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"

namespace LbbLayeredBlendBodyPart
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
		case ELbbLayeredBodyPartPoseSourceType::OutputPose:
			return TEXT("Output Pose");
		case ELbbLayeredBodyPartPoseSourceType::TemporaryPose:
			return FString::Printf(TEXT("Temp(%s)"), Source.TemporaryPoseName.IsNone() ? TEXT("<None>") : *Source.TemporaryPoseName.ToString());
		default:
			return TEXT("Unknown");
		}
	}

	static FString GetPoseTargetLabel(const FLbbLayeredBodyPartPoseTarget& Target)
	{
		switch (Target.Type)
		{
		case ELbbLayeredBodyPartPoseTargetType::OutputPose:
			return TEXT("Output Pose");
		case ELbbLayeredBodyPartPoseTargetType::TemporaryPose:
			return FString::Printf(TEXT("Temp(%s)"), Target.TemporaryPoseName.IsNone() ? TEXT("<None>") : *Target.TemporaryPoseName.ToString());
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
	
	void FCompiledOperator::BuildBoneBlendWeights(const USkeleton* InSkeleton)
	{
	}

	void FCompiledOperator::RebuildBoneBlendWeights(const FBoneContainer& RequiredBones)
	{
	}

	void FCompiledOperator::UpdateBlendWeight(const FAnimInstanceProxy* InstanceProxy)
	{
	}

	void FCompiledOperator::AccumulateCommonRequirements(
		FOperatorRequirements& Requirements,
		const FCompiledPoseSource* SourceA,
		const FCompiledPoseSource* SourceB,
		const FCompiledPoseTarget* Target)
	{
		auto MarkSourceUsage = [&Requirements](const FCompiledPoseSource* Source)
		{
			if (Source == nullptr)
			{
				return;
			}

			switch (Source->Type)
			{
			case ELbbLayeredBodyPartPoseSourceType::BasePose:
				Requirements.bNeedsBasePose = true;
				break;
			case ELbbLayeredBodyPartPoseSourceType::OverlayPose:
				Requirements.bNeedsOverlayPose = true;
				break;
			default:
				break;
			}
		};

		MarkSourceUsage(SourceA);
		MarkSourceUsage(SourceB);

		if (Target != nullptr && Target->Type == ELbbLayeredBodyPartPoseTargetType::OutputPose)
		{
			Requirements.bCanAffectOutput = true;
		}
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

	
	///
	/// Child class
	///
	
	
	class FWeightedCompiledOperator : public FCompiledOperator
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

	class FCompiledCopyPoseOperator final : public FCompiledOperator
	{
	public:
		FCompiledCopyPoseOperator(
			const FCompiledPoseSource& InSourcePose,
			const FCompiledPoseTarget& InTargetPose)
			: SourcePose(InSourcePose)
			, TargetPose(InTargetPose)
		{
		}

		virtual void AccumulateRequirements(FOperatorRequirements& Requirements) const override
		{
			AccumulateCommonRequirements(Requirements, &SourcePose, nullptr, &TargetPose);
		}

		virtual void Execute(FOperatorExecutionContext& ExecutionContext) const override
		{
			FOperatorExecutionContext::AssignTarget(
				ExecutionContext,
				TargetPose,
				FOperatorExecutionContext::ResolveSource(ExecutionContext, SourcePose));
		}

	private:
		FCompiledPoseSource SourcePose;
		FCompiledPoseTarget TargetPose;
	};

	class FCompiledApplySlotOperator final : public FCompiledOperator
	{
	public:
		FCompiledApplySlotOperator(
			const FCompiledPoseSource& InSourcePose,
			const FCompiledPoseTarget& InTargetPose,
			const int32 InSlotNodeIndex)
			: SourcePose(InSourcePose)
			, TargetPose(InTargetPose)
			, SlotNodeIndex(InSlotNodeIndex)
		{
		}

		virtual void AccumulateRequirements(FOperatorRequirements& Requirements) const override
		{
			AccumulateCommonRequirements(Requirements, &SourcePose, nullptr, &TargetPose);
			Requirements.bNeedsSlotEvaluation = true;
		}

		virtual void Execute(FOperatorExecutionContext& ExecutionContext) const override
		{
			const FPoseContext& Source = FOperatorExecutionContext::ResolveSource(ExecutionContext, SourcePose);
			FPoseContext& Target = FOperatorExecutionContext::ResolveTarget(ExecutionContext, TargetPose);
			if (!FOperatorExecutionContext::CanEvaluateSlot(ExecutionContext, SlotNodeIndex))
			{
				Target = Source;
				return;
			}

			FOperatorExecutionContext::EvaluateSlot(ExecutionContext, SlotNodeIndex, Source, Target);
		}

	private:
		FCompiledPoseSource SourcePose;
		FCompiledPoseTarget TargetPose;
		int32 SlotNodeIndex = INDEX_NONE;
	};

	class FCompiledApplyAdditiveOperator final : public FWeightedCompiledOperator
	{
	public:
		FCompiledApplyAdditiveOperator(
			const FCompiledPoseSource& InBasePose,
			const FCompiledPoseTarget& InTargetPose,
			const FLbbBlendWeight& InWeight,
			const bool bInMeshSpace)
			: FWeightedCompiledOperator(InWeight)
			, BasePose(InBasePose)
			, TargetPose(InTargetPose)
			, bMeshSpace(bInMeshSpace)
		{
		}

		virtual void AccumulateRequirements(FOperatorRequirements& Requirements) const override
		{
			AccumulateCommonRequirements(Requirements, &BasePose, nullptr, &TargetPose);
			Requirements.bNeedsBasePose = true;
			if (bMeshSpace)
			{
				Requirements.bNeedsMeshSpaceAdditive = true;
			}
			else
			{
				Requirements.bNeedsLocalSpaceAdditive = true;
			}
		}

		virtual void Execute(FOperatorExecutionContext& ExecutionContext) const override
		{
			const FPoseContext& Base = FOperatorExecutionContext::ResolveSource(ExecutionContext, BasePose);
			FPoseContext& Target = FOperatorExecutionContext::ResolveTarget(ExecutionContext, TargetPose);
			Target = Base;

			const float BlendWeight = Weight.GetBlendWeight();
			if (FAnimWeight::IsRelevant(BlendWeight))
			{
				FOperatorExecutionContext::AccumulateAdditive(ExecutionContext, Target, bMeshSpace, BlendWeight);
			}
		}

	private:
		FCompiledPoseSource BasePose;
		FCompiledPoseTarget TargetPose;
		bool bMeshSpace = false;
	};

	class FCompiledBlendTwoPosesOperator final : public FWeightedCompiledOperator
	{
	public:
		FCompiledBlendTwoPosesOperator(
			const FCompiledPoseSource& InBasePose,
			const FCompiledPoseSource& InBlendPose,
			const FCompiledPoseTarget& InTargetPose,
			const FLbbBlendWeight& InWeight)
			: FWeightedCompiledOperator(InWeight)
			, BasePose(InBasePose)
			, BlendPose(InBlendPose)
			, TargetPose(InTargetPose)
		{
		}

		virtual void AccumulateRequirements(FOperatorRequirements& Requirements) const override
		{
			AccumulateCommonRequirements(Requirements, &BasePose, &BlendPose, &TargetPose);
		}

		virtual void Execute(FOperatorExecutionContext& ExecutionContext) const override
		{
			const FPoseContext& Base = FOperatorExecutionContext::ResolveSource(ExecutionContext, BasePose);
			const FPoseContext& Blend = FOperatorExecutionContext::ResolveSource(ExecutionContext, BlendPose);
			FPoseContext& Target = FOperatorExecutionContext::ResolveTarget(ExecutionContext, TargetPose);

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

			FOperatorExecutionContext::BlendTwoPoses(ExecutionContext, Base, Blend, Target, BlendWeight);
		}

	private:
		FCompiledPoseSource BasePose;
		FCompiledPoseSource BlendPose;
		FCompiledPoseTarget TargetPose;
	};

	class FCompiledMaskedBlendOperator final : public FWeightedCompiledOperator
	{
	public:
		FCompiledMaskedBlendOperator(
			const FCompiledPoseSource& InBasePose,
			const FCompiledPoseSource& InBlendPose,
			const FCompiledPoseTarget& InTargetPose,
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

		virtual void BuildBoneBlendWeights(const USkeleton* InSkeleton) override
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

		virtual void RebuildBoneBlendWeights(const FBoneContainer& RequiredBones) override
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

		virtual void AccumulateRequirements(FOperatorRequirements& Requirements) const override
		{
			AccumulateCommonRequirements(Requirements, &BasePose, &BlendPose, &TargetPose);
		}

		virtual void Execute(FOperatorExecutionContext& ExecutionContext) const override
		{
			const FPoseContext& Base = FOperatorExecutionContext::ResolveSource(ExecutionContext, BasePose);
			const FPoseContext& Blend = FOperatorExecutionContext::ResolveSource(ExecutionContext, BlendPose);
			FPoseContext& Target = FOperatorExecutionContext::ResolveTarget(ExecutionContext, TargetPose);

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

			FPoseContext BasePoseCopy(ExecutionContext.OutputPose);
			BasePoseCopy = Base;
			FPoseContext BlendPoseCopy(ExecutionContext.OutputPose);
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

	private:
		void UpdateCurrentBlendWeights(const float BlendWeight) const
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

		FCompiledPoseSource BasePose;
		FCompiledPoseSource BlendPose;
		FCompiledPoseTarget TargetPose;
		FInputBlendPose BoneFilter;
		mutable TArray<FPerBoneBlendWeight> SkeletonBoneWeights;
		mutable TArray<FPerBoneBlendWeight> DesiredBoneWeights;
		mutable TArray<FPerBoneBlendWeight> CurrentBoneWeights;
		bool bMeshSpace = false;
	};
}

FString FLbbLayeredBlendBodyPartOperatorBase::ToString() const
{
	return TEXT("<Invalid Operator>");
}

TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> FLbbLayeredBlendBodyPartOperatorBase::CreateCompiledOperator(
	LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const
{
	ensureMsgf(false, TEXT("FLbbLayeredBlendBodyPartOperatorBase::CreateCompiledOperator should be overridden by derived operators."));
	return nullptr;
}

FString FLbbLayeredBlendBodyPartOperator_CopyPose::ToString() const
{
	return FString::Printf(
		TEXT("Copy Pose | Source: %s | Output: %s"),
		*LbbLayeredBlendBodyPart::GetPoseSourceLabel(SourcePose),
		*LbbLayeredBlendBodyPart::GetPoseTargetLabel(Output));
}

TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> FLbbLayeredBlendBodyPartOperator_CopyPose::CreateCompiledOperator(
	LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const
{
	return MakeUnique<LbbLayeredBlendBodyPart::FCompiledCopyPoseOperator>(
		CompileContext.CompileSource(SourcePose),
		CompileContext.CompileTarget(Output));
}

FString FLbbLayeredBlendBodyPartOperator_ApplySlot::ToString() const
{
	return FString::Printf(
		TEXT("Slot | Input: %s | Slot: %s | Output: %s"),
		*LbbLayeredBlendBodyPart::GetPoseSourceLabel(SourcePose),
		SlotName.IsNone() ? TEXT("<None>") : *SlotName.ToString(),
		*LbbLayeredBlendBodyPart::GetPoseTargetLabel(Output));
}

TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> FLbbLayeredBlendBodyPartOperator_ApplySlot::CreateCompiledOperator(
	LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const
{
	return MakeUnique<LbbLayeredBlendBodyPart::FCompiledApplySlotOperator>(
		CompileContext.CompileSource(SourcePose),
		CompileContext.CompileTarget(Output),
		CompileContext.FindOrAddSlotNode(SlotName));
}

FString FLbbLayeredBlendBodyPartOperator_ApplyAdditive::ToString() const
{
	return FString::Printf(
		TEXT("Apply Additive | Base: %s | Space: %s | Weight: %s | Output: %s"),
		*LbbLayeredBlendBodyPart::GetPoseSourceLabel(BasePose),
		*LbbLayeredBlendBodyPart::GetBoneSpaceLabel(AdditiveSpace),
		*LbbLayeredBlendBodyPart::GetBlendWeightLabel(Weight),
		*LbbLayeredBlendBodyPart::GetPoseTargetLabel(Output));
}

TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> FLbbLayeredBlendBodyPartOperator_ApplyAdditive::CreateCompiledOperator(
	LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const
{
	return MakeUnique<LbbLayeredBlendBodyPart::FCompiledApplyAdditiveOperator>(
		CompileContext.CompileSource(BasePose),
		CompileContext.CompileTarget(Output),
		Weight,
		AdditiveSpace == ELbbBoneSpace::MeshSpace);
}

FString FLbbLayeredBlendBodyPartOperator_BlendTwoPoses::ToString() const
{
	return FString::Printf(
		TEXT("Blend Two Poses | Base: %s | Blend: %s | Weight: %s | Output: %s"),
		*LbbLayeredBlendBodyPart::GetPoseSourceLabel(BasePose),
		*LbbLayeredBlendBodyPart::GetPoseSourceLabel(BlendPose),
		*LbbLayeredBlendBodyPart::GetBlendWeightLabel(Weight),
		*LbbLayeredBlendBodyPart::GetPoseTargetLabel(Output));
}

TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> FLbbLayeredBlendBodyPartOperator_BlendTwoPoses::CreateCompiledOperator(
	LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const
{
	return MakeUnique<LbbLayeredBlendBodyPart::FCompiledBlendTwoPosesOperator>(
		CompileContext.CompileSource(BasePose),
		CompileContext.CompileSource(BlendPose),
		CompileContext.CompileTarget(Output),
		Weight);
}

FString FLbbLayeredBlendBodyPartOperator_MaskedBlend::ToString() const
{
	return FString::Printf(
		TEXT("Layered Blend Per Bone | Base: %s | Blend: %s | Space: %s | Filters: %d | Weight: %s | Output: %s"),
		*LbbLayeredBlendBodyPart::GetPoseSourceLabel(BasePose),
		*LbbLayeredBlendBodyPart::GetPoseSourceLabel(BlendPose),
		*LbbLayeredBlendBodyPart::GetBoneSpaceLabel(BlendSpace),
		BoneFilter.BranchFilters.Num(),
		*LbbLayeredBlendBodyPart::GetBlendWeightLabel(Weight),
		*LbbLayeredBlendBodyPart::GetPoseTargetLabel(Output));
}

TUniquePtr<LbbLayeredBlendBodyPart::FCompiledOperator> FLbbLayeredBlendBodyPartOperator_MaskedBlend::CreateCompiledOperator(
	LbbLayeredBlendBodyPart::IOperatorCompileContext& CompileContext) const
{
	return MakeUnique<LbbLayeredBlendBodyPart::FCompiledMaskedBlendOperator>(
		CompileContext.CompileSource(BasePose),
		CompileContext.CompileSource(BlendPose),
		CompileContext.CompileTarget(Output),
		BoneFilter,
		Weight,
		BlendSpace == ELbbBoneSpace::MeshSpace);
}
