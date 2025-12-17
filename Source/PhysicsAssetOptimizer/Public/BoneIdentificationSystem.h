/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PhysicsOptimizerTypes.h"

class USkeletalMesh;

/**
 * 骨骼识别系统
 *
 * 使用三通道融合技术识别骨骼类型：
 * 1. 命名规则（快速路径）: 正则匹配常见骨骼命名
 * 2. 拓扑链分析（核心）: DFS 查找最长链、分叉点、末端
 * 3. 几何校验（纠偏）: 包围盒宽高比、法向突变、方向向量
 *
 * 目标：对于命名混乱的骨骼网格体也能99%正确识别关键骨骼
 */
class PHYSICSASSETOPTIMIZER_API FBoneIdentificationSystem
{
public:
	/**
	 * 三通道融合识别骨骼
	 * @param Mesh 骨骼网格体
	 * @return 骨骼名称到骨骼类型的映射
	 */
	static TMap<FName, EBoneType> IdentifyBones(USkeletalMesh* Mesh);

	/**
	 * 查找特定类型的骨骼
	 * @param BoneTypes 骨骼类型映射
	 * @param Type 要查找的骨骼类型
	 * @return 该类型的所有骨骼名称
	 */
	static TArray<FName> FindBonesByType(
		const TMap<FName, EBoneType>& BoneTypes,
		EBoneType Type
	);

private:
	// ========== 通道1: 命名规则 ==========

	/**
	 * 通过命名规则识别骨骼
	 * 使用正则模式匹配常见骨骼命名（spine, head, clavicle等）
	 */
	static TMap<FName, EBoneType> IdentifyByNaming(const FReferenceSkeleton& Skel);

	/**
	 * 检查骨骼名称是否匹配特定类型
	 */
	static bool MatchesBoneType(const FString& BoneName, EBoneType Type);

	// ========== 通道2: 拓扑链分析 ==========

	/**
	 * 通过拓扑链分析识别骨骼
	 * DFS 查找最长链（脊柱）、分叉点（四肢）、末端（手脚）
	 */
	static TMap<FName, EBoneType> IdentifyByTopology(const FReferenceSkeleton& Skel);

	/**
	 * 查找最长连续父子链
	 * @param Skel 骨架
	 * @param StartBoneIndex 起始骨骼索引
	 * @param OutChain 输出骨骼链
	 * @return 链的长度
	 */
	static int32 FindLongestChain(
		const FReferenceSkeleton& Skel,
		int32 StartBoneIndex,
		TArray<int32>& OutChain
	);

	/**
	 * DFS 遍历骨骼树，计算深度和子节点数
	 */
	static void AnalyzeBoneTree(
		const FReferenceSkeleton& Skel,
		int32 BoneIndex,
		TMap<int32, int32>& OutDepths,
		TMap<int32, int32>& OutChildCounts
	);

	/**
	 * 识别四肢链（从脊柱分叉点开始的长链）
	 */
	static TArray<TArray<int32>> IdentifyLimbChains(
		const FReferenceSkeleton& Skel,
		const TArray<int32>& SpineChain
	);

	// ========== 通道3: 几何校验 ==========

	/**
	 * 使用几何特征验证和纠正识别结果
	 * 检查包围盒、顶点分布、骨骼方向等
	 */
	static void ValidateWithGeometry(
		USkeletalMesh* Mesh,
		TMap<FName, EBoneType>& InOutBoneTypes
	);

	/**
	 * 检查是否为头部骨骼（包围盒宽高比接近1）
	 */
	static bool IsHeadBone(USkeletalMesh* Mesh, int32 BoneIndex);

	/**
	 * 检查是否为袖口/领口骨骼（短长度 + 法向突变）
	 */
	static bool IsCuffBone(USkeletalMesh* Mesh, int32 BoneIndex);

	// ========== 辅助函数 ==========

	/**
	 * 计算骨骼长度
	 */
	static float CalculateBoneLength(
		const FReferenceSkeleton& Skel,
		int32 BoneIndex
	);

	/**
	 * 获取骨骼的直接子骨骼
	 */
	static TArray<int32> GetDirectChildren(
		const FReferenceSkeleton& Skel,
		int32 BoneIndex
	);

	/**
	 * 检查骨骼是否为末端（无子骨骼）
	 */
	static bool IsTerminalBone(
		const FReferenceSkeleton& Skel,
		int32 BoneIndex
	);

	/**
	 * 获取骨骼在世界空间的 Z 坐标（用于区分上下肢）
	 */
	static float GetBoneWorldZ(
		const FReferenceSkeleton& Skel,
		int32 BoneIndex
	);
};
