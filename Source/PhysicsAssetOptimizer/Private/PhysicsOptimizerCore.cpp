/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "PhysicsOptimizerCore.h"
#include "PhysicsAssetUtils.h"
#include "BoneIdentificationSystem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "MeshUtilitiesCommon.h"
#include "MeshUtilitiesEngine.h"

// 性能分析
#include "ProfilingDebugging/ScopedTimers.h"

#define LOCTEXT_NAMESPACE "PhysicsOptimizerCore"

bool FPhysicsOptimizerCore::OptimizePhysicsAsset(
	UPhysicsAsset* PA,
	USkeletalMesh* Mesh,
	const FPhysicsOptimizerSettings& Settings,
	FPhysicsOptimizerStats& OutStats)
{
	if (!PA || !Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[物理资产优化器] 无效的物理资产或骨骼网格体"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 开始优化: %s"), *PA->GetName());

	// 性能计时
	FScopedDurationTimer Timer(OutStats.OptimizationTimeMs);

	// 计算优化前统计
	CalculatePreOptimizationStats(PA, OutStats);

	// ========== 骨骼识别 ==========
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 执行骨骼识别..."));
	TMap<FName, EBoneType> BoneTypes = FBoneIdentificationSystem::IdentifyBones(Mesh);

	// ========== 重建物理形体（UE官方最佳实践）==========
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 重建物理形体..."));
	RebuildPhysicsBodies(PA, Mesh, BoneTypes, Settings);

	// ========== 配置物理参数 ==========
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 配置物理参数..."));
	
	ConfigureDamping(PA, Settings.LinearDamping, Settings.AngularDamping);
	ConfigureMassDistribution(PA, Mesh, Settings.BaseMass);
	ConfigureConstraintLimits(PA, BoneTypes);

	if (Settings.bConfigureSleep)
	{
		ConfigureSleepSettings(PA, Settings.SleepThreshold);
	}

	// 计算优化后统计
	CalculatePostOptimizationStats(PA, OutStats);

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 优化完成! Body: %d->%d, 碰撞对: %d->%d, 耗时: %.2fms"),
		OutStats.OriginalBodyCount, OutStats.FinalBodyCount,
		OutStats.OriginalCollisionPairs, OutStats.FinalCollisionPairs,
		OutStats.OptimizationTimeMs);

	return true;
}

// ========== 核心重建函数 ==========

void FPhysicsOptimizerCore::RebuildPhysicsBodies(
	UPhysicsAsset* PA,
	USkeletalMesh* Mesh,
	const TMap<FName, EBoneType>& BoneTypes,
	const FPhysicsOptimizerSettings& Settings)
{
	if (!PA || !Mesh)
	{
		return;
	}

	// 清空所有现有物理形体和约束
	while (PA->SkeletalBodySetups.Num() > 0)
	{
		FPhysicsAssetUtils::DestroyBody(PA, 0);
	}
	PA->ConstraintSetup.Empty();
	
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 已清空现有物理形体"));

	// 使用 UE 官方 API 计算每个骨骼的顶点信息
	TArray<FBoneVertInfo> BoneVertInfos;
	FMeshUtilitiesEngine::CalcBoneVertInfos(Mesh, BoneVertInfos, false);
	
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 计算了 %d 个骨骼的顶点信息"), BoneVertInfos.Num());

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	int32 CreatedCount = 0;

	for (const auto& Pair : BoneTypes)
	{
		const FName& BoneName = Pair.Key;
		const EBoneType BoneType = Pair.Value;

		// 根据配置的规则检查是否需要创建物理形体
		const FBonePhysicsRule& Rule = Settings.GetRuleForBoneType(BoneType);
		if (!Rule.bCreateBody)
		{
			continue;
		}

		// 检查骨骼是否存在
		const int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			continue;
		}

		// 检查是否应该跳过该骨骼
		if (ShouldSkipBone(RefSkel, BoneIndex, BoneName, BoneType, Settings.MinBoneSize))
		{
			continue;
		}

		// 获取该骨骼的顶点信息
		const FBoneVertInfo& VertInfo = (BoneIndex < BoneVertInfos.Num()) 
			? BoneVertInfos[BoneIndex] 
			: FBoneVertInfo();

		if (CreateBodyForBoneUsingUEAPI(PA, Mesh, BoneName, BoneType, Rule.ShapeType, VertInfo))
		{
			CreatedCount++;
			UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 创建: %s (类型=%d, 形状=%d, 顶点=%d)"),
				*BoneName.ToString(), static_cast<int32>(BoneType), 
				static_cast<int32>(Rule.ShapeType), VertInfo.Positions.Num());
		}
	}

	// 更新索引映射和碰撞设置
	PA->UpdateBodySetupIndexMap();
	PA->UpdateBoundsBodiesArray();
	PA->InvalidateAllPhysicsMeshes();

	// 创建约束（连接父子骨骼）
	CreateConstraints(PA, Mesh);
	
	// 标记资产已修改
	PA->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 创建了 %d 个物理形体"), CreatedCount);
}

bool FPhysicsOptimizerCore::CreateBodyForBoneUsingUEAPI(
	UPhysicsAsset* PA,
	USkeletalMesh* Mesh,
	const FName& BoneName,
	EBoneType BoneType,
	EPhysicsShapeType ShapeType,
	const FBoneVertInfo& VertInfo)
{
	if (!PA || !Mesh)
	{
		return false;
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	const int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE)
	{
		return false;
	}

	// 检查是否已存在该骨骼的 Body
	if (PA->FindBodyIndex(BoneName) != INDEX_NONE)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] %s 已存在物理形体，跳过"), *BoneName.ToString());
		return false;
	}

	// 配置 UE 官方的物理形体创建参数
	FPhysAssetCreateParams CreateParams;
	CreateParams.MinBoneSize = 1.0f;
	CreateParams.VertWeight = EVW_AnyWeight;
	CreateParams.bAutoOrientToBone = true;  // 使用协方差矩阵自动对齐骨骼方向
	CreateParams.bCreateConstraints = false;
	
	// 根据配置的形状类型设置 UE 参数
	switch (ShapeType)
	{
	case EPhysicsShapeType::Box:
		CreateParams.GeomType = EFG_Box;
		break;
	case EPhysicsShapeType::Sphere:
		CreateParams.GeomType = EFG_Sphere;
		break;
	case EPhysicsShapeType::Convex:
		CreateParams.GeomType = EFG_SingleConvexHull;
		break;
	case EPhysicsShapeType::Capsule:
	default:
		CreateParams.GeomType = EFG_Sphyl;
		break;
	}

	// 1. 创建空的 BodySetup
	int32 NewBodyIndex = FPhysicsAssetUtils::CreateNewBody(PA, BoneName, CreateParams);
	if (NewBodyIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] 创建 %s BodySetup 失败"), *BoneName.ToString());
		return false;
	}

	USkeletalBodySetup* BodySetup = PA->SkeletalBodySetups[NewBodyIndex].Get();
	if (!BodySetup)
	{
		return false;
	}

	// 2. 使用 UE 官方 API 创建碰撞形状（自动计算尺寸和方向）
	bool bSuccess = false;
	
	if (VertInfo.Positions.Num() > 0)
	{
		// 有顶点信息，使用官方 API
		bSuccess = FPhysicsAssetUtils::CreateCollisionFromBone(
			BodySetup, 
			Mesh, 
			BoneIndex, 
			CreateParams, 
			VertInfo
		);
		
		if (bSuccess)
		{
			// 检查是否成功创建了碰撞形状
			int32 ShapeCount = BodySetup->AggGeom.SphylElems.Num() + 
				BodySetup->AggGeom.BoxElems.Num() + 
				BodySetup->AggGeom.SphereElems.Num();
			
			if (ShapeCount == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] %s: CreateCollisionFromBone 返回成功但没有创建形状"), 
					*BoneName.ToString());
				bSuccess = false;
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] %s: 官方API创建了 %d 个形状"), 
					*BoneName.ToString(), ShapeCount);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] %s: CreateCollisionFromBone 失败"), 
				*BoneName.ToString());
		}
	}
	
	if (!bSuccess)
	{
		// 回退：基于骨骼长度创建简单形状
		UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] %s 使用回退方案"), *BoneName.ToString());
		
		float BoneLength = GetBoneLength(RefSkel, BoneIndex);
		BoneLength = FMath::Max(BoneLength, 10.0f);
		
		CreateFallbackShape(BodySetup, ShapeType, BoneLength);
		bSuccess = true;
	}

	// 刷新物理数据
	BodySetup->InvalidatePhysicsData();
	BodySetup->CreatePhysicsMeshes();

	return bSuccess;
}

void FPhysicsOptimizerCore::CreateFallbackShape(
	UBodySetup* BodySetup,
	EPhysicsShapeType ShapeType,
	float BoneLength)
{
	if (!BodySetup)
	{
		return;
	}

	switch (ShapeType)
	{
	case EPhysicsShapeType::Box:
		{
			FKBoxElem Box;
			Box.Center = FVector(BoneLength * 0.5f, 0, 0);
			Box.X = BoneLength;
			Box.Y = BoneLength * 0.6f;
			Box.Z = BoneLength * 0.3f;
			BodySetup->AggGeom.BoxElems.Add(Box);
		}
		break;

	case EPhysicsShapeType::Sphere:
		{
			FKSphereElem Sphere;
			Sphere.Center = FVector(BoneLength * 0.5f, 0, 0);
			Sphere.Radius = BoneLength * 0.4f;
			BodySetup->AggGeom.SphereElems.Add(Sphere);
		}
		break;

	case EPhysicsShapeType::Capsule:
	default:
		{
			FKSphylElem Capsule;
			Capsule.Center = FVector(BoneLength * 0.5f, 0, 0);
			Capsule.Rotation = FRotator(0, 0, 90);
			Capsule.Radius = BoneLength * 0.15f;
			Capsule.Length = FMath::Max(BoneLength - Capsule.Radius * 2.0f, BoneLength * 0.5f);
			BodySetup->AggGeom.SphylElems.Add(Capsule);
		}
		break;
	}

	BodySetup->InvalidatePhysicsData();
	BodySetup->CreatePhysicsMeshes();
}



float FPhysicsOptimizerCore::GetBoneLength(
	const FReferenceSkeleton& RefSkel,
	int32 BoneIndex)
{
	float MaxLength = 0.0f;

	// 查找子骨骼计算长度
	for (int32 ChildIndex = 0; ChildIndex < RefSkel.GetNum(); ++ChildIndex)
	{
		if (RefSkel.GetParentIndex(ChildIndex) == BoneIndex)
		{
			const FVector ChildLocation = RefSkel.GetRefBonePose()[ChildIndex].GetLocation();
			MaxLength = FMath::Max(MaxLength, ChildLocation.Size());
		}
	}

	return MaxLength;
}

// ========== P0 规则实现 ==========

void FPhysicsOptimizerCore::ConfigureDamping(
	UPhysicsAsset* PA,
	float LinearDamping,
	float AngularDamping)
{
	if (!PA)
	{
		return;
	}

	int32 ConfiguredCount = 0;

	for (const auto& BodySetupPtr : PA->SkeletalBodySetups)
	{
		USkeletalBodySetup* BodySetup = BodySetupPtr.Get();
		if (!BodySetup)
		{
			continue;
		}

		// 通过 DefaultInstance 设置阻尼
		FBodyInstance& DefaultInstance = BodySetup->DefaultInstance;
		DefaultInstance.LinearDamping = LinearDamping;
		DefaultInstance.AngularDamping = AngularDamping;
		
		ConfiguredCount++;
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 配置了 %d 个骨骼的阻尼 (Linear: %.2f, Angular: %.2f)"),
		ConfiguredCount, LinearDamping, AngularDamping);
}

void FPhysicsOptimizerCore::ConfigureMassDistribution(
	UPhysicsAsset* PA,
	USkeletalMesh* Mesh,
	float BaseMass)
{
	if (!PA || !Mesh)
	{
		return;
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();

	// 计算每个骨骼的深度
	TMap<int32, int32> BoneDepths;
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		int32 Depth = 0;
		int32 CurrentBone = i;

		// 向上遍历到根骨骼计算深度
		while (CurrentBone != INDEX_NONE)
		{
			const int32 ParentIndex = RefSkel.GetParentIndex(CurrentBone);
			if (ParentIndex == INDEX_NONE)
			{
				break;
			}
			Depth++;
			CurrentBone = ParentIndex;

			// 防止无限循环
			if (Depth > 100)
			{
				UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] Rule03: 骨骼层级过深，可能存在循环引用"));
				break;
			}
		}

		BoneDepths.Add(i, Depth);
	}

	int32 ConfiguredCount = 0;

	// 为每个 Body 设置质量
	for (const auto& BodySetupPtr : PA->SkeletalBodySetups)
	{
		USkeletalBodySetup* BodySetup = BodySetupPtr.Get();
		if (!BodySetup)
		{
			continue;
		}

		const FName BoneName = BodySetup->BoneName;
		const int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			continue;
		}

		const int32* DepthPtr = BoneDepths.Find(BoneIndex);
		if (!DepthPtr)
		{
			continue;
		}

		const int32 Depth = *DepthPtr;

		// 计算质量：BaseMass / 2^Depth
		// 例如：根骨骼 80kg，深度1 → 40kg，深度2 → 20kg
		const float Mass = BaseMass / FMath::Pow(2.0f, static_cast<float>(Depth));

		// 设置质量缩放（通过 PhysicsType 和质量覆盖）
		BodySetup->PhysicsType = EPhysicsType::PhysType_Simulated;

		// 注意：UE 的质量计算基于形状体积和密度
		// 通过 FBodyInstance 设置质量覆盖
		FBodyInstance& DefaultInstance = BodySetup->DefaultInstance;
		DefaultInstance.bOverrideMass = true;
		DefaultInstance.SetMassOverride(Mass, true);

		ConfiguredCount++;

		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] Rule03: %s (深度=%d) → 质量=%.2fkg"),
			*BoneName.ToString(), Depth, Mass);
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 配置了 %d 个骨骼的质量分布"), ConfiguredCount);
}

void FPhysicsOptimizerCore::ConfigureConstraintLimits(
	UPhysicsAsset* PA,
	const TMap<FName, EBoneType>& BoneTypes)
{
	if (!PA || BoneTypes.Num() == 0)
	{
		return;
	}

	int32 ConfiguredCount = 0;

	for (int32 i = 0; i < PA->ConstraintSetup.Num(); ++i)
	{
		UPhysicsConstraintTemplate* Constraint = PA->ConstraintSetup[i];
		if (!Constraint)
		{
			continue;
		}

		FConstraintInstance& DefaultInstance = Constraint->DefaultInstance;

		// 获取约束连接的两个骨骼名称
		const FName Bone1Name = DefaultInstance.ConstraintBone1;
		const FName Bone2Name = DefaultInstance.ConstraintBone2;

		// 查找骨骼类型
		const EBoneType* Bone1TypePtr = BoneTypes.Find(Bone1Name);
		const EBoneType* Bone2TypePtr = BoneTypes.Find(Bone2Name);

		if (!Bone1TypePtr && !Bone2TypePtr)
		{
			continue;
		}

		// 使用任意一个已识别的骨骼类型
		const EBoneType BoneType = Bone1TypePtr ? *Bone1TypePtr : *Bone2TypePtr;

		// 根据骨骼类型设置约束限制
		switch (BoneType)
		{
		case EBoneType::Arm:
		case EBoneType::Leg:
			// 四肢：适度限制
			DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 30.0f);
			DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 15.0f);
			DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 45.0f);
			ConfiguredCount++;
			break;

		case EBoneType::Spine:
			// 脊柱：严格限制
			DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 15.0f);
			DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 10.0f);
			DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 20.0f);
			ConfiguredCount++;
			break;

		case EBoneType::Finger:
			// 手指：宽松限制
			DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 60.0f);
			DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 30.0f);
			DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 10.0f);
			ConfiguredCount++;
			break;

		case EBoneType::Tail:
			// 尾巴：非常宽松
			DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
			DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
			DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 30.0f);
			ConfiguredCount++;
			break;

		default:
			// 其他骨骼：保持默认
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 配置了 %d 个约束限制"), ConfiguredCount);
}

void FPhysicsOptimizerCore::ConfigureSleepSettings(
	UPhysicsAsset* PA,
	float Threshold)
{
	if (!PA)
	{
		return;
	}

	int32 ConfiguredCount = 0;

	for (const auto& BodySetupPtr : PA->SkeletalBodySetups)
	{
		USkeletalBodySetup* BodySetup = BodySetupPtr.Get();
		if (!BodySetup)
		{
			continue;
		}

		// 配置默认实例的 Sleep 设置
		FBodyInstance& DefaultInstance = BodySetup->DefaultInstance;

		// 设置 Sleep Family 为 Custom
		DefaultInstance.SleepFamily = ESleepFamily::Custom;

		// 设置自定义 Sleep 阈值倍数
		DefaultInstance.CustomSleepThresholdMultiplier = Threshold;

		// 启用自动睡眠
		DefaultInstance.bGenerateWakeEvents = false;
		DefaultInstance.bStartAwake = true;

		ConfiguredCount++;
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 配置了 %d 个骨骼的 Sleep 设置 (阈值=%.3f)"),
		ConfiguredCount, Threshold);
}

// ========== 统计函数 ==========

void FPhysicsOptimizerCore::CalculatePreOptimizationStats(
	UPhysicsAsset* PA,
	FPhysicsOptimizerStats& OutStats)
{
	if (!PA)
	{
		return;
	}

	OutStats.OriginalBodyCount = PA->SkeletalBodySetups.Num();
	OutStats.OriginalCollisionPairs = CountCollisionPairs(PA);
}

void FPhysicsOptimizerCore::CalculatePostOptimizationStats(
	UPhysicsAsset* PA,
	FPhysicsOptimizerStats& OutStats)
{
	if (!PA)
	{
		return;
	}

	OutStats.FinalBodyCount = PA->SkeletalBodySetups.Num();
	OutStats.FinalCollisionPairs = CountCollisionPairs(PA);
	OutStats.RemovedSmallBones = OutStats.OriginalBodyCount - OutStats.FinalBodyCount;
}

int32 FPhysicsOptimizerCore::CountCollisionPairs(UPhysicsAsset* PA)
{
	if (!PA)
	{
		return 0;
	}

	int32 Count = 0;
	const int32 BodyCount = PA->SkeletalBodySetups.Num();

	for (int32 i = 0; i < BodyCount; ++i)
	{
		for (int32 j = i + 1; j < BodyCount; ++j)
		{
			if (PA->IsCollisionEnabled(i, j))
			{
				Count++;
			}
		}
	}

	return Count;
}

void FPhysicsOptimizerCore::CreateConstraints(UPhysicsAsset* PA, USkeletalMesh* Mesh)
{
	if (!PA || !Mesh)
	{
		return;
	}

	// 检查是否允许创建约束
	if (!FPhysicsAssetUtils::CanCreateConstraints())
	{
		UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] 约束创建被禁用"));
		return;
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	int32 CreatedCount = 0;

	// 为每个物理形体创建与父骨骼的约束
	for (int32 BodyIndex = 0; BodyIndex < PA->SkeletalBodySetups.Num(); ++BodyIndex)
	{
		USkeletalBodySetup* BodySetup = PA->SkeletalBodySetups[BodyIndex].Get();
		if (!BodySetup)
		{
			continue;
		}

		const FName BoneName = BodySetup->BoneName;
		const int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			continue;
		}

		// 查找父骨骼（向上遍历直到找到有物理形体的骨骼）
		int32 ParentBoneIndex = RefSkel.GetParentIndex(BoneIndex);
		FName ParentBodyBoneName = NAME_None;
		int32 ParentBodyIndex = INDEX_NONE;

		while (ParentBoneIndex != INDEX_NONE)
		{
			FName ParentBoneName = RefSkel.GetBoneName(ParentBoneIndex);
			ParentBodyIndex = PA->FindBodyIndex(ParentBoneName);
			if (ParentBodyIndex != INDEX_NONE)
			{
				ParentBodyBoneName = ParentBoneName;
				break;
			}
			ParentBoneIndex = RefSkel.GetParentIndex(ParentBoneIndex);
		}

		// 如果没有找到父物理形体，跳过
		if (ParentBodyBoneName == NAME_None)
		{
			continue;
		}

		// 使用 UE 官方 API 创建约束
		int32 NewConstraintIndex = FPhysicsAssetUtils::CreateNewConstraint(PA, BoneName);
		if (NewConstraintIndex == INDEX_NONE)
		{
			continue;
		}

		UPhysicsConstraintTemplate* Constraint = PA->ConstraintSetup[NewConstraintIndex];
		if (!Constraint)
		{
			continue;
		}

		FConstraintInstance& DefaultInstance = Constraint->DefaultInstance;

		// 设置角度约束模式（默认 Limited）
		DefaultInstance.SetAngularSwing1Motion(EAngularConstraintMotion::ACM_Limited);
		DefaultInstance.SetAngularSwing2Motion(EAngularConstraintMotion::ACM_Limited);
		DefaultInstance.SetAngularTwistMotion(EAngularConstraintMotion::ACM_Limited);

		// 设置约束连接的两个骨骼
		DefaultInstance.ConstraintBone1 = BoneName;
		DefaultInstance.ConstraintBone2 = ParentBodyBoneName;

		// 使用官方 API 自动对齐约束位置
		DefaultInstance.SnapTransformsToDefault(EConstraintTransformComponentFlags::All, PA);

		// 设置默认配置
		Constraint->SetDefaultProfile(DefaultInstance);

		// 禁用约束骨骼间的碰撞
		PA->DisableCollision(BodyIndex, ParentBodyIndex);

		CreatedCount++;

		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] 创建约束: %s -> %s"),
			*BoneName.ToString(), *ParentBodyBoneName.ToString());
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 创建了 %d 个约束"), CreatedCount);
}

bool FPhysicsOptimizerCore::ShouldSkipBone(
	const FReferenceSkeleton& RefSkel,
	int32 BoneIndex,
	const FName& BoneName,
	EBoneType BoneType,
	float MinBoneSize)
{
	// 1. 排除真正的根骨骼（没有父骨骼）
	if (RefSkel.GetParentIndex(BoneIndex) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] 跳过: %s (根骨骼)"), *BoneName.ToString());
		return true;
	}

	// 2. 排除细分骨骼和不需要物理的骨骼
	FString BoneNameLower = BoneName.ToString().ToLower();
	
	// 细分骨骼（Part1, Part2 等）
	if (BoneNameLower.Contains(TEXT("part")))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] 跳过: %s (细分骨骼)"), *BoneName.ToString());
		return true;
	}
	
	// 末端骨骼（End）
	if (BoneNameLower.Contains(TEXT("end")))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] 跳过: %s (末端骨骼)"), *BoneName.ToString());
		return true;
	}
	
	// 脚趾
	if (BoneNameLower.Contains(TEXT("toe")))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] 跳过: %s (脚趾)"), *BoneName.ToString());
		return true;
	}
	
	// 手指
	if (BoneNameLower.Contains(TEXT("finger")) || BoneNameLower.Contains(TEXT("thumb")) ||
	    BoneNameLower.Contains(TEXT("index")) || BoneNameLower.Contains(TEXT("middle")) ||
	    BoneNameLower.Contains(TEXT("ring")) || BoneNameLower.Contains(TEXT("pinky")))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] 跳过: %s (手指)"), *BoneName.ToString());
		return true;
	}

	// 3. 排除武器/道具骨骼（通常命名不符合人形骨骼规范）
	// 检查是否包含常见的人形骨骼关键词（不能只靠后缀判断）
	bool bIsHumanoidBone = 
		BoneNameLower.Contains(TEXT("spine")) || BoneNameLower.Contains(TEXT("chest")) ||
		BoneNameLower.Contains(TEXT("neck")) || BoneNameLower.Contains(TEXT("head")) ||
		BoneNameLower.Contains(TEXT("shoulder")) || BoneNameLower.Contains(TEXT("elbow")) ||
		BoneNameLower.Contains(TEXT("wrist")) || BoneNameLower.Contains(TEXT("hand")) ||
		BoneNameLower.Contains(TEXT("hip")) || BoneNameLower.Contains(TEXT("knee")) ||
		BoneNameLower.Contains(TEXT("ankle")) || BoneNameLower.Contains(TEXT("foot")) ||
		BoneNameLower.Contains(TEXT("clavicle")) || BoneNameLower.Contains(TEXT("scapula")) ||
		BoneNameLower.Contains(TEXT("pelvis")) || BoneNameLower.Contains(TEXT("root_")) ||
		BoneNameLower.Contains(TEXT("arm")) || BoneNameLower.Contains(TEXT("leg")) ||
		BoneNameLower.Contains(TEXT("thigh")) || BoneNameLower.Contains(TEXT("calf"));
	
	// 后缀 _L/_R/_M 只有在已经是人形骨骼时才有意义，不能单独作为判断依据
	
	if (!bIsHumanoidBone)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] 跳过: %s (非人形骨骼)"), *BoneName.ToString());
		return true;
	}

	// 4. 检查骨骼长度（太小的骨骼跳过，但手脚除外）
	float BoneLength = GetBoneLength(RefSkel, BoneIndex);
	if (BoneLength < MinBoneSize && BoneType != EBoneType::Hand && BoneType != EBoneType::Foot)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] 跳过: %s (长度%.1f < %.1f)"), 
			*BoneName.ToString(), BoneLength, MinBoneSize);
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
