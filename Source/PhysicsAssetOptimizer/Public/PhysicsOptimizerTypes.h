/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PhysicsOptimizerTypes.generated.h"

/**
 * 骨骼类型枚举
 * 用于骨骼识别系统的分类结果
 */
UENUM(BlueprintType)
enum class EBoneType : uint8
{
	Unknown       UMETA(DisplayName = "未知"),
	Spine         UMETA(DisplayName = "脊柱"),
	Head          UMETA(DisplayName = "头部"),
	Arm           UMETA(DisplayName = "手臂"),
	Leg           UMETA(DisplayName = "腿部"),
	Hand          UMETA(DisplayName = "手"),
	Foot          UMETA(DisplayName = "脚"),
	Finger        UMETA(DisplayName = "手指"),
	Tail          UMETA(DisplayName = "尾巴"),
	Clavicle      UMETA(DisplayName = "锁骨"),
	Pelvis        UMETA(DisplayName = "骨盆"),
	Face          UMETA(DisplayName = "面部")
};

/**
 * 物理形体形状类型
 */
UENUM(BlueprintType)
enum class EPhysicsShapeType : uint8
{
	Capsule   UMETA(DisplayName = "胶囊体"),
	Box       UMETA(DisplayName = "盒体"),
	Sphere    UMETA(DisplayName = "球体"),
	Convex    UMETA(DisplayName = "凸包")
};

/**
 * 单个骨骼类型的物理形体规则
 */
USTRUCT(BlueprintType)
struct FBonePhysicsRule
{
	GENERATED_BODY()

	FBonePhysicsRule()
		: ShapeType(EPhysicsShapeType::Capsule)
		, bCreateBody(true)
	{}

	FBonePhysicsRule(EPhysicsShapeType InShape, bool bCreate = true)
		: ShapeType(InShape)
		, bCreateBody(bCreate)
	{}

	/** 使用的形状类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "规则")
	EPhysicsShapeType ShapeType;

	/** 是否为该类型骨骼创建物理形体 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "规则")
	bool bCreateBody;
};

/**
 * 物理资产优化设置
 */
USTRUCT(BlueprintType)
struct FPhysicsOptimizerSettings
{
	GENERATED_BODY()

	FPhysicsOptimizerSettings()
		: MinBoneSize(8.0f)
		, BaseMass(80.0f)
		, LinearDamping(0.2f)
		, AngularDamping(0.8f)
		, TerminalBoneLinearDamping(1.0f)
		, LongBoneThreshold(40.0f)
		, SleepThreshold(0.05f)
		, LevelSetResolution(64)
		, bUseLSV(false)
		, bUseConvexForLongBones(true)
		, bConfigureSleep(true)
	{
		// 初始化默认物理形体规则（UE最佳实践）
		InitDefaultBoneRules();
	}

	/** 初始化默认骨骼物理规则 */
	void InitDefaultBoneRules()
	{
		// 核心骨骼：使用胶囊体
		BoneRules.Add(EBoneType::Pelvis,   FBonePhysicsRule(EPhysicsShapeType::Capsule));
		BoneRules.Add(EBoneType::Spine,    FBonePhysicsRule(EPhysicsShapeType::Capsule));
		BoneRules.Add(EBoneType::Head,     FBonePhysicsRule(EPhysicsShapeType::Capsule));
		BoneRules.Add(EBoneType::Clavicle, FBonePhysicsRule(EPhysicsShapeType::Capsule));
		BoneRules.Add(EBoneType::Arm,      FBonePhysicsRule(EPhysicsShapeType::Capsule));
		BoneRules.Add(EBoneType::Leg,      FBonePhysicsRule(EPhysicsShapeType::Capsule));
		
		// 手脚：使用盒体（更好地包裹扁平形状）
		BoneRules.Add(EBoneType::Hand,     FBonePhysicsRule(EPhysicsShapeType::Box));
		BoneRules.Add(EBoneType::Foot,     FBonePhysicsRule(EPhysicsShapeType::Box));
		
		// 不创建物理形体的骨骼类型
		BoneRules.Add(EBoneType::Finger,   FBonePhysicsRule(EPhysicsShapeType::Capsule, false));
		BoneRules.Add(EBoneType::Face,     FBonePhysicsRule(EPhysicsShapeType::Sphere, false));
		BoneRules.Add(EBoneType::Tail,     FBonePhysicsRule(EPhysicsShapeType::Capsule, true));
		BoneRules.Add(EBoneType::Unknown,  FBonePhysicsRule(EPhysicsShapeType::Capsule, false));
	}

	/** 获取骨骼类型对应的物理规则 */
	const FBonePhysicsRule& GetRuleForBoneType(EBoneType BoneType) const
	{
		if (const FBonePhysicsRule* Rule = BoneRules.Find(BoneType))
		{
			return *Rule;
		}
		static FBonePhysicsRule DefaultRule(EPhysicsShapeType::Capsule, false);
		return DefaultRule;
	}

	/** 忽略长度小于此值的末端骨骼（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "优化设置")
	float MinBoneSize;

	/** 基础质量（kg），会按父节点递减 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "优化设置")
	float BaseMass;

	/** 线性阻尼（通用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "优化设置")
	float LinearDamping;

	/** 角度阻尼（通用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "优化设置")
	float AngularDamping;

	/** 末端骨骼线性阻尼（消除面条感） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "优化设置")
	float TerminalBoneLinearDamping;

	/** 长骨阈值，超过此长度使用凸包替代胶囊（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "优化设置")
	float LongBoneThreshold;

	/** Sleep 阈值（CustomSleepThresholdMultiplier） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "优化设置")
	float SleepThreshold;

	/** Level Set Volume 分辨率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "高级设置")
	int32 LevelSetResolution;

	/** 是否生成 LSV（Level Set Volume）用于布料碰撞 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "高级设置")
	bool bUseLSV;

	/** 是否对长骨使用多凸包 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "高级设置")
	bool bUseConvexForLongBones;

	/** 是否配置 Sleep 设置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "高级设置")
	bool bConfigureSleep;

	/** 每种骨骼类型的物理形体规则 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "形体规则")
	TMap<EBoneType, FBonePhysicsRule> BoneRules;
};

/**
 * 优化统计信息
 */
USTRUCT(BlueprintType)
struct FPhysicsOptimizerStats
{
	GENERATED_BODY()

	FPhysicsOptimizerStats()
		: OriginalBodyCount(0)
		, FinalBodyCount(0)
		, OriginalCollisionPairs(0)
		, FinalCollisionPairs(0)
		, RemovedSmallBones(0)
		, OptimizationTimeMs(0.0f)
	{}

	/** 原始 Body 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "统计")
	int32 OriginalBodyCount;

	/** 优化后 Body 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "统计")
	int32 FinalBodyCount;

	/** 原始碰撞对数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "统计")
	int32 OriginalCollisionPairs;

	/** 优化后碰撞对数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "统计")
	int32 FinalCollisionPairs;

	/** 移除的小骨骼数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "统计")
	int32 RemovedSmallBones;

	/** 优化耗时（毫秒） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "统计")
	double OptimizationTimeMs;
};
