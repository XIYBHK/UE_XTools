/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PhysicsOptimizerTypes.h"

class UPhysicsAsset;
class USkeletalMesh;

/**
 * 物理资产优化核心
 *
 * 实现12条硬规则，自动优化物理资产：
 * P0: Rule01, 04, 06, 07 - 性能优化基础
 * P1: Rule02, 03, 05, 12 - 形状和约束优化
 * P2: Rule08, 09, 10, 11 - 高级功能
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

	// ========== P0 优化规则（高优先级）==========

	/**
	 * Rule01: 移除小骨骼
	 * 忽略长度 < MinBoneSize 的末端骨骼，减少30%无效Body
	 */
	static void Rule01_RemoveSmallBones(
		UPhysicsAsset* PA,
		USkeletalMesh* Mesh,
		float MinBoneSize = 8.0f
	);

	/**
	 * Rule04: 配置阻尼
	 * 线阻尼0.2，角阻尼0.8，抑制抖动
	 */
	static void Rule04_ConfigureDamping(
		UPhysicsAsset* PA,
		float LinearDamping = 0.2f,
		float AngularDamping = 0.8f
	);

	/**
	 * Rule06: 禁用核心骨骼碰撞
	 * 锁骨-骨盆与根节点禁用碰撞，防炸开
	 */
	static void Rule06_DisableCoreCollisions(
		UPhysicsAsset* PA,
		const TArray<FName>& CoreBones
	);

	/**
	 * Rule07: 禁用细节骨骼碰撞
	 * 手指/面部全部禁用碰撞，再减50%碰撞对
	 */
	static void Rule07_DisableFineDetailCollisions(
		UPhysicsAsset* PA,
		const TArray<FName>& FingerBones,
		const TArray<FName>& FaceBones
	);

	// ========== P1 优化规则（中优先级）==========

	/**
	 * Rule02: 优化碰撞形状
	 * 脊柱单胶囊；四肢2-3胶囊；手脚球体
	 */
	static void Rule02_OptimizeShapes(
		UPhysicsAsset* PA,
		const TMap<FName, EBoneType>& BoneTypes
	);

	/**
	 * Rule03: 配置质量分布
	 * 质量按父节点一半递减，重心稳定
	 */
	static void Rule03_ConfigureMassDistribution(
		UPhysicsAsset* PA,
		USkeletalMesh* Mesh,
		float BaseMass = 80.0f
	);

	/**
	 * Rule05: 配置约束角度限制
	 * 四肢 Swing1=30°/Swing2=15°/Twist=45°；脊柱Swing1=15°
	 */
	static void Rule05_ConfigureConstraintLimits(
		UPhysicsAsset* PA,
		const TMap<FName, EBoneType>& BoneTypes
	);

	/**
	 * Rule12: 配置 Sleep 设置
	 * Sleep Family=Custom，阈值0.05，落地秒停，省25%CPU
	 */
	static void Rule12_ConfigureSleepSettings(
		UPhysicsAsset* PA,
		float Threshold = 0.05f
	);

	// ========== P2 优化规则（低优先级/高级功能）==========

	/**
	 * Rule08: 配置末端骨骼阻尼
	 * 末端骨骼线阻尼=1.0，消除面条感
	 */
	static void Rule08_ConfigureTerminalBoneDamping(
		UPhysicsAsset* PA,
		USkeletalMesh* Mesh
	);

	/**
	 * Rule09: 形状自动对齐骨骼方向
	 * 引擎自带API自动对齐（验证功能）
	 */
	static void Rule09_AlignShapesToBones(UPhysicsAsset* PA);

	/**
	 * Rule10: 生成 LSV（Level Set Volume）
	 * 领口/袖口生成LSV(分辨率64)，布料零穿模
	 *
	 * @warning 使用内部 API LevelSetHelpers::CreateLevelSetForBone
	 */
	static void Rule10_GenerateLSV(
		UPhysicsAsset* PA,
		USkeletalMesh* Mesh,
		const TArray<FName>& CuffBones,
		int32 Resolution = 64
	);

	/**
	 * Rule11: 长骨使用多凸包
	 * 长骨>40cm用4凸包替代单胶囊，贴合肌肉
	 */
	static void Rule11_UseConvexForLongBones(
		UPhysicsAsset* PA,
		USkeletalMesh* Mesh,
		float LengthThreshold = 40.0f
	);

private:
	/** 计算优化前的统计数据 */
	static void CalculatePreOptimizationStats(
		UPhysicsAsset* PA,
		FPhysicsOptimizerStats& OutStats
	);

	/** 计算优化后的统计数据 */
	static void CalculatePostOptimizationStats(
		UPhysicsAsset* PA,
		FPhysicsOptimizerStats& OutStats
	);

	/** 统计碰撞对数量 */
	static int32 CountCollisionPairs(UPhysicsAsset* PA);
};
