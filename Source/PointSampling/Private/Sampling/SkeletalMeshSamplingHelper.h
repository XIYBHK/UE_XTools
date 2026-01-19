/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

class USkeletalMesh;

/**
 * 骨骼网格体采样辅助类
 *
 * 负责从骨骼网格体提取骨骼数据，无需创建临时组件
 * 直接从 USkeletalMesh::GetRefSkeleton() 获取骨骼信息
 */
class FSkeletalMeshSamplingHelper
{
public:
	/**
	 * 从骨骼网格体生成骨骼变换数据
	 *
	 * @param SkeletalMesh 骨骼网格体资产
	 * @param Transform 应用到每个骨骼的世界变换
	 * @param Config 采样配置
	 * @return 骨骼变换数据数组
	 */
	static TArray<FBoneTransformData> GenerateFromSkeletalBones(
		USkeletalMesh* SkeletalMesh,
		const FTransform& Transform,
		const FSkeletalBoneSamplingConfig& Config
	);

private:
	/**
	 * 骨骼信息结构体
	 * 用于构建骨骼层次结构
	 */
	struct FBoneInfo
	{
		FName Name;
		int32 Index;
		int32 ParentIndex;
		int32 Depth;
		bool IsLeaf;

		FBoneInfo()
			: Name(NAME_None)
			, Index(INDEX_NONE)
			, ParentIndex(INDEX_NONE)
			, Depth(0)
			, IsLeaf(false)
		{
		}
	};

	/**
	 * 构建骨骼层次结构
	 * 计算每个骨骼的深度、父子关系、是否为叶子节点
	 */
	static TArray<FBoneInfo> BuildBoneHierarchy(const FReferenceSkeleton& RefSkeleton);

	/**
	 * 根据采样模式过滤骨骼
	 * 返回符合条件的骨骼索引数组
	 */
	static TArray<int32> FilterBonesByMode(
		const TArray<FBoneInfo>& BoneInfos,
		const FSkeletalBoneSamplingConfig& Config
	);

	/**
	 * 计算骨骼深度
	 * 递归计算从根骨骼到指定骨骼的层级深度
	 */
	static int32 CalculateBoneDepth(int32 BoneIndex, const FReferenceSkeleton& RefSkeleton);

	/**
	 * 判断骨骼是否为叶子节点
	 * 叶子节点是指没有子骨骼的节点
	 */
	static bool IsLeafBone(int32 BoneIndex, const FReferenceSkeleton& RefSkeleton);
};
