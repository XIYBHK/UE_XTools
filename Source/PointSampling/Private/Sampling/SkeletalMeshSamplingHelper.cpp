/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "SkeletalMeshSamplingHelper.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"

TArray<FBoneTransformData> FSkeletalMeshSamplingHelper::GenerateFromSkeletalBones(
	USkeletalMesh* SkeletalMesh,
	const FTransform& Transform,
	const FSkeletalBoneSamplingConfig& Config)
{
	TArray<FBoneTransformData> Result;

	if (!SkeletalMesh)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[骨骼采样] 骨骼网格体为空"));
		return Result;
	}

	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	const int32 BoneCount = RefSkeleton.GetNum();

	if (BoneCount == 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[骨骼采样] 骨骼网格体没有骨骼"));
		return Result;
	}

	// 构建骨骼层次结构
	TArray<FBoneInfo> BoneInfos = BuildBoneHierarchy(RefSkeleton);

	// 根据配置过滤骨骼
	TArray<int32> FilteredIndices = FilterBonesByMode(BoneInfos, Config);

	if (FilteredIndices.Num() == 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[骨骼采样] 没有符合条件的骨骼"));
		return Result;
	}

	// 获取参考姿态的骨骼变换数组
	const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();

	// 为每个符合条件的骨骼创建变换数据
	Result.Reserve(FilteredIndices.Num());
	for (int32 BoneIndex : FilteredIndices)
	{
		if (!RefBonePose.IsValidIndex(BoneIndex))
		{
			continue;
		}

		FBoneTransformData BoneData;
		BoneData.BoneIndex = BoneIndex;
		BoneData.BoneName = BoneInfos[BoneIndex].Name;

		// 获取参考姿态的变换
		FTransform BoneTransform = RefBonePose[BoneIndex];

		// 根据配置决定是否包含旋转
		if (!Config.bIncludeRotation)
		{
			BoneTransform.SetRotation(FQuat::Identity);
		}

		// 应用用户提供的变换
		if (Config.bApplyRefPoseTransform)
		{
			BoneTransform = BoneTransform * Transform;
		}
		else
		{
			// 不应用参考姿态变换，仅使用位置
			BoneTransform.SetTranslation(Transform.TransformPosition(BoneTransform.GetTranslation()));
		}

		BoneData.Transform = BoneTransform;
		Result.Add(MoveTemp(BoneData));
	}

	UE_LOG(LogPointSampling, Log, TEXT("[骨骼采样] 从 %d 个骨骼中提取了 %d 个骨骼"),
		BoneCount, Result.Num());

	return Result;
}

TArray<FSkeletalMeshSamplingHelper::FBoneInfo> FSkeletalMeshSamplingHelper::BuildBoneHierarchy(
	const FReferenceSkeleton& RefSkeleton)
{
	const int32 BoneCount = RefSkeleton.GetNum();
	TArray<FBoneInfo> BoneInfos;
	BoneInfos.SetNum(BoneCount);

	for (int32 i = 0; i < BoneCount; ++i)
	{
		FBoneInfo& Info = BoneInfos[i];
		Info.Index = i;
		Info.Name = RefSkeleton.GetBoneName(i);
		Info.ParentIndex = RefSkeleton.GetParentIndex(i);
		Info.Depth = CalculateBoneDepth(i, RefSkeleton);
		Info.IsLeaf = IsLeafBone(i, RefSkeleton);
	}

	return BoneInfos;
}

TArray<int32> FSkeletalMeshSamplingHelper::FilterBonesByMode(
	const TArray<FBoneInfo>& BoneInfos,
	const FSkeletalBoneSamplingConfig& Config)
{
	TArray<int32> Result;

	for (const FBoneInfo& BoneInfo : BoneInfos)
	{
		bool bInclude = false;

		switch (Config.SamplingMode)
		{
		case ESkeletalBoneSamplingMode::AllBones:
			bInclude = true;
			break;

		case ESkeletalBoneSamplingMode::MajorBones:
			// 仅包含根骨骼（深度0）和直接子骨骼（深度1）
			bInclude = (BoneInfo.Depth <= 1);
			break;

		case ESkeletalBoneSamplingMode::LeafBones:
			bInclude = BoneInfo.IsLeaf;
			break;

		case ESkeletalBoneSamplingMode::ByDepth:
			bInclude = (BoneInfo.Depth <= Config.MaxDepth);
			break;

		case ESkeletalBoneSamplingMode::ByNamePrefix:
			if (!Config.BoneNamePrefix.IsEmpty())
			{
				bInclude = BoneInfo.Name.ToString().StartsWith(Config.BoneNamePrefix);
			}
			break;

		case ESkeletalBoneSamplingMode::CustomList:
			bInclude = Config.CustomBoneNames.Contains(BoneInfo.Name);
			break;
		}

		if (bInclude)
		{
			Result.Add(BoneInfo.Index);
		}
	}

	return Result;
}

int32 FSkeletalMeshSamplingHelper::CalculateBoneDepth(int32 BoneIndex, const FReferenceSkeleton& RefSkeleton)
{
	int32 Depth = 0;
	int32 CurrentIndex = BoneIndex;

	while (CurrentIndex != INDEX_NONE)
	{
		const int32 ParentIndex = RefSkeleton.GetParentIndex(CurrentIndex);
		if (ParentIndex == INDEX_NONE || ParentIndex == CurrentIndex)
		{
			break;
		}
		CurrentIndex = ParentIndex;
		++Depth;
	}

	return Depth;
}

bool FSkeletalMeshSamplingHelper::IsLeafBone(int32 BoneIndex, const FReferenceSkeleton& RefSkeleton)
{
	// FReferenceSkeleton 没有 GetChildren 方法
	// 需要遍历所有骨骼，检查是否有骨骼的父索引等于当前骨骼索引
	const int32 BoneCount = RefSkeleton.GetNum();
	for (int32 i = 0; i < BoneCount; ++i)
	{
		if (RefSkeleton.GetParentIndex(i) == BoneIndex)
		{
			return false; // 找到子骨骼，不是叶子节点
		}
	}
	return true; // 没有子骨骼，是叶子节点
}
