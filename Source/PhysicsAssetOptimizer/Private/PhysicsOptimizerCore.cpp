/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "PhysicsOptimizerCore.h"
#include "PhysicsAssetUtils.h"
#include "BoneIdentificationSystem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "Rendering/SkeletalMeshRenderData.h"

// 性能分析
#include "ProfilingDebugging/ScopedTimers.h"

// LSV 生成（来自 PhysicsUtilities Private 目录）
// 注意：这是内部 API，未来版本可能变更
#if WITH_EDITOR
	// LevelSetHelpers 在 Developer/PhysicsUtilities/Private 中
	// 由于是 Private 头文件，可能在某些情况下不可用
	// 如果编译失败，可以注释掉 Rule10 的实现
	//#include "LevelSetHelpers.h"
#endif

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

	// 只为核心骨骼类型生成物理形体
	TSet<EBoneType> CoreTypes = {
		EBoneType::Pelvis,    // 骨盆
		EBoneType::Spine,     // 脊椎
		EBoneType::Head,      // 头
		EBoneType::Clavicle,  // 锁骨
		EBoneType::Arm,       // 大臂/小臂
		EBoneType::Leg,       // 大腿/小腿
		EBoneType::Hand,      // 手
		EBoneType::Foot       // 脚
	};

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	int32 CreatedCount = 0;

	for (const auto& Pair : BoneTypes)
	{
		const FName& BoneName = Pair.Key;
		const EBoneType BoneType = Pair.Value;

		// 只处理核心骨骼类型
		if (!CoreTypes.Contains(BoneType))
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

		CreateBodyForBone(PA, Mesh, BoneName, BoneType);
		CreatedCount++;
		
		UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 创建: %s (类型=%d)"),
			*BoneName.ToString(), static_cast<int32>(BoneType));
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

void FPhysicsOptimizerCore::CreateBodyForBone(
	UPhysicsAsset* PA,
	USkeletalMesh* Mesh,
	const FName& BoneName,
	EBoneType BoneType)
{
	if (!PA || !Mesh)
	{
		return;
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	const int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE)
	{
		return;
	}

	// 检查是否已存在该骨骼的 Body
	if (PA->FindBodyIndex(BoneName) != INDEX_NONE)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] %s 已存在物理形体，跳过"), *BoneName.ToString());
		return;
	}

	// 创建 BodySetup
	USkeletalBodySetup* BodySetup = NewObject<USkeletalBodySetup>(PA, NAME_None, RF_Transactional);
	if (!BodySetup)
	{
		UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] 无法为 %s 创建物理形体"), *BoneName.ToString());
		return;
	}

	// 初始化 BodySetup
	BodySetup->BoneName = BoneName;
	BodySetup->PhysicsType = EPhysicsType::PhysType_Default;
	BodySetup->CollisionTraceFlag = CTF_UseDefault;
	BodySetup->CollisionReponse = EBodyCollisionResponse::BodyCollision_Enabled;
	
	// 添加到物理资产
	PA->SkeletalBodySetups.Add(BodySetup);

	// 计算骨骼尺寸（优先使用顶点包围盒）
	FVector BoneSize;
	FVector BoneCenter;
	bool bHasBounds = CalculateBoneBounds(Mesh, BoneIndex, BoneSize, BoneCenter);
	
	if (!bHasBounds)
	{
		// 回退：使用子骨骼距离
		float BoneLength = GetBoneLength(RefSkel, BoneIndex);
		BoneLength = FMath::Max(BoneLength, 10.0f);
		BoneSize = FVector(BoneLength, BoneLength * 0.3f, BoneLength * 0.3f);
		BoneCenter = FVector(BoneLength * 0.5f, 0, 0);
	}

	// 确保最小尺寸
	BoneSize.X = FMath::Max(BoneSize.X, 5.0f);
	BoneSize.Y = FMath::Max(BoneSize.Y, 3.0f);
	BoneSize.Z = FMath::Max(BoneSize.Z, 3.0f);

	// 根据骨骼类型创建形状
	if (BoneType == EBoneType::Hand || BoneType == EBoneType::Foot)
	{
		// 手脚：使用盒体（上上版逻辑，你说满意的）
		FKBoxElem Box;
		Box.Center = BoneCenter;
		Box.Rotation = FRotator::ZeroRotator;
		Box.X = BoneSize.X;
		Box.Y = BoneSize.Y;
		Box.Z = BoneSize.Z;
		
		BodySetup->AggGeom.BoxElems.Add(Box);
	}
	else
	{
		// 其他骨骼：使用胶囊体
		FKSphylElem Capsule;
		Capsule.Center = BoneCenter;
		Capsule.Rotation = FRotator(0, 0, 90); // 沿骨骼 X 轴方向
		
		// 胶囊体总长度 = BoneSize.X（沿骨骼方向的尺寸）
		float TotalLength = BoneSize.X;
		
		// 半径 = 横截面的一半，但不能超过总长度的 25%
		float CrossSection = FMath::Max(BoneSize.Y, BoneSize.Z);
		Capsule.Radius = FMath::Min(CrossSection * 0.5f, TotalLength * 0.25f);
		Capsule.Radius = FMath::Max(Capsule.Radius, 2.0f); // 最小半径
		
		// 头部特殊处理：更圆
		if (BoneType == EBoneType::Head)
		{
			Capsule.Radius = FMath::Max(TotalLength, CrossSection) * 0.35f;
		}
		
		// 圆柱部分长度 = 总长度 - 两端半球
		// 确保至少有 50% 是圆柱部分
		Capsule.Length = FMath::Max(TotalLength - Capsule.Radius * 2.0f, TotalLength * 0.5f);
		
		BodySetup->AggGeom.SphylElems.Add(Capsule);
		
		UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] %s: Size=(%.1f,%.1f,%.1f), Radius=%.1f, Length=%.1f"),
			*BoneName.ToString(), BoneSize.X, BoneSize.Y, BoneSize.Z, Capsule.Radius, Capsule.Length);
	}

	BodySetup->InvalidatePhysicsData();
	BodySetup->CreatePhysicsMeshes();
}

bool FPhysicsOptimizerCore::CalculateBoneBounds(
	USkeletalMesh* Mesh,
	int32 BoneIndex,
	FVector& OutSize,
	FVector& OutCenter)
{
	if (!Mesh || BoneIndex == INDEX_NONE)
	{
		return false;
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	
	// 获取骨骼的世界变换（用于将顶点转换到骨骼局部空间）
	TArray<FMatrix> ComponentSpaceTransforms;
	ComponentSpaceTransforms.SetNum(RefSkel.GetNum());
	
	// 计算所有骨骼的组件空间变换
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		const FTransform& LocalTransform = RefSkel.GetRefBonePose()[i];
		int32 ParentIndex = RefSkel.GetParentIndex(i);
		
		if (ParentIndex == INDEX_NONE)
		{
			ComponentSpaceTransforms[i] = LocalTransform.ToMatrixWithScale();
		}
		else
		{
			ComponentSpaceTransforms[i] = LocalTransform.ToMatrixWithScale() * ComponentSpaceTransforms[ParentIndex];
		}
	}

	// 获取骨骼变换的逆矩阵（用于将顶点转换到骨骼局部空间）
	FMatrix BoneToComponent = ComponentSpaceTransforms[BoneIndex];
	FMatrix ComponentToBone = BoneToComponent.Inverse();

	// 获取渲染数据
	FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
	if (!RenderData || RenderData->LODRenderData.Num() == 0)
	{
		return false;
	}

	const FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[0];
	const FSkinWeightVertexBuffer* SkinWeightBuffer = LODData.GetSkinWeightVertexBuffer();
	const FPositionVertexBuffer& PositionBuffer = LODData.StaticVertexBuffers.PositionVertexBuffer;

	if (!SkinWeightBuffer || PositionBuffer.GetNumVertices() == 0)
	{
		return false;
	}

	// 收集受该骨骼影响的顶点（权重 > 0.1）
	FBox BoneBounds(ForceInit);
	int32 VertexCount = 0;
	const float MinWeight = 0.1f;

	for (uint32 VertexIndex = 0; VertexIndex < PositionBuffer.GetNumVertices(); ++VertexIndex)
	{
		// 检查该顶点是否受当前骨骼影响
		bool bInfluenced = false;
		float TotalWeight = 0.0f;

		// 获取骨骼权重（最多 MAX_TOTAL_INFLUENCES 个影响）
		int32 SectionIndex = 0;
		int32 VertexIndexInSection = VertexIndex;
		
		// 查找顶点所在的 Section
		for (int32 i = 0; i < LODData.RenderSections.Num(); ++i)
		{
			const FSkelMeshRenderSection& Section = LODData.RenderSections[i];
			if (VertexIndex >= Section.BaseVertexIndex && 
			    VertexIndex < Section.BaseVertexIndex + Section.NumVertices)
			{
				SectionIndex = i;
				VertexIndexInSection = VertexIndex - Section.BaseVertexIndex;
				break;
			}
		}

		const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIndex];
		
		// 检查骨骼影响
		for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex)
		{
			int32 InfluenceBoneIndex = SkinWeightBuffer->GetBoneIndex(VertexIndex, InfluenceIndex);
			uint8 InfluenceWeight = SkinWeightBuffer->GetBoneWeight(VertexIndex, InfluenceIndex);
			
			if (InfluenceWeight == 0)
			{
				continue;
			}

			// 将 Section 的骨骼映射转换为骨架骨骼索引
			if (InfluenceBoneIndex < Section.BoneMap.Num())
			{
				int32 SkeletonBoneIndex = Section.BoneMap[InfluenceBoneIndex];
				if (SkeletonBoneIndex == BoneIndex)
				{
					float Weight = static_cast<float>(InfluenceWeight) / 255.0f;
					if (Weight >= MinWeight)
					{
						bInfluenced = true;
						TotalWeight = Weight;
						break;
					}
				}
			}
		}

		if (bInfluenced)
		{
			// 获取顶点位置并转换到骨骼局部空间
			FVector3f VertexPos = PositionBuffer.VertexPosition(VertexIndex);
			FVector WorldPos = FVector(VertexPos.X, VertexPos.Y, VertexPos.Z);
			FVector LocalPos = ComponentToBone.TransformPosition(WorldPos);
			
			BoneBounds += LocalPos;
			VertexCount++;
		}
	}

	// 如果没有找到足够的顶点，返回失败
	if (VertexCount < 3)
	{
		return false;
	}

	// 计算尺寸和中心
	OutSize = BoneBounds.GetSize();
	OutCenter = BoneBounds.GetCenter();

	UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] %s 包围盒: 顶点=%d, 尺寸=(%.1f, %.1f, %.1f)"),
		*RefSkel.GetBoneName(BoneIndex).ToString(), VertexCount, OutSize.X, OutSize.Y, OutSize.Z);

	return true;
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

	for (USkeletalBodySetup* BodySetup : PA->SkeletalBodySetups)
	{
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
	for (USkeletalBodySetup* BodySetup : PA->SkeletalBodySetups)
	{
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

	for (USkeletalBodySetup* BodySetup : PA->SkeletalBodySetups)
	{
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

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	int32 CreatedCount = 0;

	// 为每个物理形体创建与父骨骼的约束
	for (int32 BodyIndex = 0; BodyIndex < PA->SkeletalBodySetups.Num(); ++BodyIndex)
	{
		USkeletalBodySetup* BodySetup = PA->SkeletalBodySetups[BodyIndex];
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

		while (ParentBoneIndex != INDEX_NONE)
		{
			FName ParentBoneName = RefSkel.GetBoneName(ParentBoneIndex);
			if (PA->FindBodyIndex(ParentBoneName) != INDEX_NONE)
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

		// 创建约束
		UPhysicsConstraintTemplate* Constraint = NewObject<UPhysicsConstraintTemplate>(PA, NAME_None, RF_Transactional);
		if (!Constraint)
		{
			continue;
		}

		// 设置约束连接的两个骨骼
		FConstraintInstance& DefaultInstance = Constraint->DefaultInstance;
		DefaultInstance.ConstraintBone1 = BoneName;
		DefaultInstance.ConstraintBone2 = ParentBodyBoneName;

		// 设置默认约束参数
		DefaultInstance.SetDisableCollision(true);
		DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
		DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
		DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 45.0f);

		PA->ConstraintSetup.Add(Constraint);
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
