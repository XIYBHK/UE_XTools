/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PhysicsOptimizerTypes.h"

class UPhysicsAsset;
class USkeletalMesh;
struct FBoneVertInfo;

/**
 * 物理资产优化核心
 *
 * 按照 UE 官方最佳实践重建物理资产：
 * - 清空现有物理形体
 * - 只为核心骨骼生成简单形状（胶囊体/盒体）
 * - 配置阻尼、质量、约束限制等参数
 */
class PHYSICSASSETOPTIMIZER_API FPhysicsOptimizerCore
{
public:
	/**
	 * 一键优化物理资产
	 * @param PA 要优化的物理资产
	 * @param Mesh 骨骼网格体
	 * @param Settings 优化设置
	 * @param OutStats 输出统计信息
	 * @return 优化是否成功
	 */
	static bool OptimizePhysicsAsset(
		UPhysicsAsset* PA,
		USkeletalMesh* Mesh,
		const FPhysicsOptimizerSettings& Settings,
		FPhysicsOptimizerStats& OutStats
	);

private:
	/** 重建物理形体（核心函数） */
	static void RebuildPhysicsBodies(
		UPhysicsAsset* PA,
		USkeletalMesh* Mesh,
		const TMap<FName, EBoneType>& BoneTypes,
		const FPhysicsOptimizerSettings& Settings
	);

	/** 使用 UE 官方 API 为单个骨骼创建物理形体 */
	static bool CreateBodyForBoneUsingUEAPI(
		UPhysicsAsset* PA,
		USkeletalMesh* Mesh,
		const FName& BoneName,
		EBoneType BoneType,
		EPhysicsShapeType ShapeType,
		const FBoneVertInfo& VertInfo
	);

	/** 创建回退形状（当官方 API 失败时） */
	static void CreateFallbackShape(
		UBodySetup* BodySetup,
		EPhysicsShapeType ShapeType,
		float BoneLength
	);



	/** 计算骨骼长度（回退方案） */
	static float GetBoneLength(const FReferenceSkeleton& RefSkel, int32 BoneIndex);

	/** 配置阻尼参数 */
	static void ConfigureDamping(UPhysicsAsset* PA, float LinearDamping, float AngularDamping);

	/** 配置质量分布 */
	static void ConfigureMassDistribution(UPhysicsAsset* PA, USkeletalMesh* Mesh, float BaseMass);

	/** 配置约束限制 */
	static void ConfigureConstraintLimits(UPhysicsAsset* PA, const TMap<FName, EBoneType>& BoneTypes);

	/** 配置 Sleep 设置 */
	static void ConfigureSleepSettings(UPhysicsAsset* PA, float Threshold);

	/** 检查是否应该跳过该骨骼 */
	static bool ShouldSkipBone(
		const FReferenceSkeleton& RefSkel,
		int32 BoneIndex,
		const FName& BoneName,
		EBoneType BoneType,
		float MinBoneSize
	);

	/** 创建约束（连接父子骨骼） */
	static void CreateConstraints(UPhysicsAsset* PA, USkeletalMesh* Mesh);

	/** 统计函数 */
	static void CalculatePreOptimizationStats(UPhysicsAsset* PA, FPhysicsOptimizerStats& OutStats);
	static void CalculatePostOptimizationStats(UPhysicsAsset* PA, FPhysicsOptimizerStats& OutStats);
	static int32 CountCollisionPairs(UPhysicsAsset* PA);
};
