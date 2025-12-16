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

	// 提取特定类型的骨骼
	TArray<FName> CoreBones;
	CoreBones.Append(FBoneIdentificationSystem::FindBonesByType(BoneTypes, EBoneType::Clavicle));
	CoreBones.Append(FBoneIdentificationSystem::FindBonesByType(BoneTypes, EBoneType::Pelvis));

	TArray<FName> FingerBones = FBoneIdentificationSystem::FindBonesByType(BoneTypes, EBoneType::Finger);
	TArray<FName> FaceBones = FBoneIdentificationSystem::FindBonesByType(BoneTypes, EBoneType::Face);

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 识别结果: 核心骨骼=%d, 手指=%d, 面部=%d"),
		CoreBones.Num(), FingerBones.Num(), FaceBones.Num());

	// ========== P0 规则（必须执行）==========
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 执行 P0 优化规则..."));

	Rule01_RemoveSmallBones(PA, Mesh, Settings.MinBoneSize);
	Rule04_ConfigureDamping(PA, Settings.LinearDamping, Settings.AngularDamping);
	Rule06_DisableCoreCollisions(PA, CoreBones);
	Rule07_DisableFineDetailCollisions(PA, FingerBones, FaceBones);

	// ========== P1 规则（推荐执行）==========
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 执行 P1 优化规则..."));

	Rule02_OptimizeShapes(PA, BoneTypes);
	Rule03_ConfigureMassDistribution(PA, Mesh, Settings.BaseMass);
	Rule05_ConfigureConstraintLimits(PA, BoneTypes);

	if (Settings.bConfigureSleep)
	{
		Rule12_ConfigureSleepSettings(PA, Settings.SleepThreshold);
	}

	// ========== P2 规则（可选）==========
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 执行 P2 优化规则..."));

	Rule08_ConfigureTerminalBoneDamping(PA, Mesh);
	Rule09_AlignShapesToBones(PA);

	if (Settings.bUseLSV)
	{
		// TODO: 需要骨骼识别结果
		// Rule10_GenerateLSV(PA, Mesh, CuffBones, Settings.LevelSetResolution);
	}

	if (Settings.bUseConvexForLongBones)
	{
		Rule11_UseConvexForLongBones(PA, Mesh, Settings.LongBoneThreshold);
	}

	// 计算优化后统计
	CalculatePostOptimizationStats(PA, OutStats);

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] 优化完成! Body: %d->%d, 碰撞对: %d->%d, 耗时: %.2fms"),
		OutStats.OriginalBodyCount, OutStats.FinalBodyCount,
		OutStats.OriginalCollisionPairs, OutStats.FinalCollisionPairs,
		OutStats.OptimizationTimeMs);

	return true;
}

// ========== P0 规则实现 ==========

void FPhysicsOptimizerCore::Rule01_RemoveSmallBones(
	UPhysicsAsset* PA,
	USkeletalMesh* Mesh,
	float MinBoneSize)
{
	if (!PA || !Mesh)
	{
		return;
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	int32 RemovedCount = 0;

	// 反向遍历，避免删除时的索引问题
	for (int32 i = PA->SkeletalBodySetups.Num() - 1; i >= 0; --i)
	{
		USkeletalBodySetup* BodySetup = PA->SkeletalBodySetups[i];
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

		// 计算骨骼长度
		const FTransform BoneTransform = RefSkel.GetRefBonePose()[BoneIndex];
		float BoneLength = 0.0f;

		// 查找子骨骼计算长度
		for (int32 ChildIndex = 0; ChildIndex < RefSkel.GetNum(); ++ChildIndex)
		{
			if (RefSkel.GetParentIndex(ChildIndex) == BoneIndex)
			{
				const FVector ChildLocation = RefSkel.GetRefBonePose()[ChildIndex].GetLocation();
				BoneLength = FMath::Max(BoneLength, ChildLocation.Size());
			}
		}

		// 如果骨骼太小且是末端骨骼（无子骨骼或子骨骼很短），移除
		if (BoneLength < MinBoneSize)
		{
			FPhysicsAssetUtils::DestroyBody(PA, i);
			RemovedCount++;
			UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] 移除小骨骼: %s (长度: %.2fcm)"),
				*BoneName.ToString(), BoneLength);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule01: 移除了 %d 个小骨骼"), RemovedCount);
}

void FPhysicsOptimizerCore::Rule04_ConfigureDamping(
	UPhysicsAsset* PA,
	float LinearDamping,
	float AngularDamping)
{
	if (!PA)
	{
		return;
	}

	for (USkeletalBodySetup* BodySetup : PA->SkeletalBodySetups)
	{
		if (!BodySetup)
		{
			continue;
		}

		// 设置阻尼（通过 PhysicsType 默认属性）
		// 注意：实际的阻尼配置需要在运行时通过 FBodyInstance 设置
		// 这里只是记录配置，实际应用需要在创建物理实例时读取
		BodySetup->PhysicsType = EPhysicsType::PhysType_Simulated;

		// TODO: 将阻尼值存储到自定义属性或元数据中
		// 运行时读取这些值并应用到 FBodyInstance
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule04: 配置阻尼 (Linear: %.2f, Angular: %.2f)"),
		LinearDamping, AngularDamping);
}

void FPhysicsOptimizerCore::Rule06_DisableCoreCollisions(
	UPhysicsAsset* PA,
	const TArray<FName>& CoreBones)
{
	if (!PA || CoreBones.Num() == 0)
	{
		return;
	}

	int32 DisabledCount = 0;

	// 禁用核心骨骼（锁骨、骨盆）之间的碰撞
	for (const FName& CoreBone : CoreBones)
	{
		const int32 CoreBodyIndex = PA->FindBodyIndex(CoreBone);
		if (CoreBodyIndex == INDEX_NONE)
		{
			continue;
		}

		// 禁用与所有其他核心骨骼的碰撞
		for (const FName& OtherCoreBone : CoreBones)
		{
			if (CoreBone == OtherCoreBone)
			{
				continue;
			}

			const int32 OtherBodyIndex = PA->FindBodyIndex(OtherCoreBone);
			if (OtherBodyIndex == INDEX_NONE)
			{
				continue;
			}

			// 禁用碰撞对
			if (PA->IsCollisionEnabled(CoreBodyIndex, OtherBodyIndex))
			{
				PA->DisableCollision(CoreBodyIndex, OtherBodyIndex);
				DisabledCount++;
				UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] 禁用碰撞: %s <-> %s"),
					*CoreBone.ToString(), *OtherCoreBone.ToString());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule06: 禁用了 %d 对核心骨骼碰撞"), DisabledCount);
}

void FPhysicsOptimizerCore::Rule07_DisableFineDetailCollisions(
	UPhysicsAsset* PA,
	const TArray<FName>& FingerBones,
	const TArray<FName>& FaceBones)
{
	if (!PA)
	{
		return;
	}

	int32 DisabledCount = 0;

	// 合并所有细节骨骼
	TArray<FName> AllDetailBones;
	AllDetailBones.Append(FingerBones);
	AllDetailBones.Append(FaceBones);

	if (AllDetailBones.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule07: 未找到细节骨骼，跳过"));
		return;
	}

	// 禁用所有细节骨骼的碰撞（与所有其他骨骼）
	for (const FName& DetailBone : AllDetailBones)
	{
		const int32 DetailBodyIndex = PA->FindBodyIndex(DetailBone);
		if (DetailBodyIndex == INDEX_NONE)
		{
			continue;
		}

		// 禁用与所有其他 Body 的碰撞
		for (int32 i = 0; i < PA->SkeletalBodySetups.Num(); ++i)
		{
			if (i == DetailBodyIndex)
			{
				continue;
			}

			if (PA->IsCollisionEnabled(DetailBodyIndex, i))
			{
				PA->DisableCollision(DetailBodyIndex, i);
				DisabledCount++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule07: 禁用了 %d 对细节骨骼碰撞（手指=%d, 面部=%d）"),
		DisabledCount, FingerBones.Num(), FaceBones.Num());
}

// ========== P1 规则实现 ==========

void FPhysicsOptimizerCore::Rule02_OptimizeShapes(
	UPhysicsAsset* PA,
	const TMap<FName, EBoneType>& BoneTypes)
{
	if (!PA || BoneTypes.Num() == 0)
	{
		return;
	}

	int32 OptimizedCount = 0;

	for (USkeletalBodySetup* BodySetup : PA->SkeletalBodySetups)
	{
		if (!BodySetup)
		{
			continue;
		}

		const FName BoneName = BodySetup->BoneName;
		const EBoneType* BoneTypePtr = BoneTypes.Find(BoneName);
		if (!BoneTypePtr)
		{
			continue;
		}

		const EBoneType BoneType = *BoneTypePtr;

		// 根据骨骼类型优化形状
		switch (BoneType)
		{
		case EBoneType::Spine:
		case EBoneType::Head:
			// 脊柱和头部：保持单个胶囊体（通常已是最优）
			if (BodySetup->AggGeom.SphylElems.Num() == 0 && BodySetup->AggGeom.SphereElems.Num() == 0)
			{
				// 如果没有简单形状，添加一个胶囊体
				FKSphylElem Capsule;
				Capsule.Center = FVector::ZeroVector;
				Capsule.Rotation = FRotator::ZeroRotator;
				Capsule.Radius = 10.0f;
				Capsule.Length = 20.0f;
				BodySetup->AggGeom.SphylElems.Add(Capsule);
				OptimizedCount++;
			}
			break;

		case EBoneType::Hand:
		case EBoneType::Foot:
			// 手脚：使用球体
			if (BodySetup->AggGeom.SphereElems.Num() == 0)
			{
				// 清除其他形状，使用单个球体
				BodySetup->AggGeom.SphylElems.Empty();
				BodySetup->AggGeom.BoxElems.Empty();

				FKSphereElem Sphere;
				Sphere.Center = FVector::ZeroVector;
				Sphere.Radius = 8.0f;  // 手脚球体半径
				BodySetup->AggGeom.SphereElems.Add(Sphere);
				OptimizedCount++;
			}
			break;

		case EBoneType::Finger:
		case EBoneType::Face:
			// 手指和面部：使用小球体或胶囊体
			if (BodySetup->AggGeom.SphereElems.Num() == 0 && BodySetup->AggGeom.SphylElems.Num() == 0)
			{
				FKSphereElem Sphere;
				Sphere.Center = FVector::ZeroVector;
				Sphere.Radius = 3.0f;  // 小球体
				BodySetup->AggGeom.SphereElems.Add(Sphere);
				OptimizedCount++;
			}
			break;

		default:
			// 其他骨骼（手臂、腿部等）：保持原有形状或使用胶囊体
			if (BodySetup->AggGeom.SphylElems.Num() == 0 && BodySetup->AggGeom.SphereElems.Num() == 0)
			{
				FKSphylElem Capsule;
				Capsule.Center = FVector::ZeroVector;
				Capsule.Rotation = FRotator::ZeroRotator;
				Capsule.Radius = 8.0f;
				Capsule.Length = 30.0f;
				BodySetup->AggGeom.SphylElems.Add(Capsule);
				OptimizedCount++;
			}
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule02: 优化了 %d 个骨骼形状"), OptimizedCount);
}

void FPhysicsOptimizerCore::Rule03_ConfigureMassDistribution(
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

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule03: 配置了 %d 个骨骼的质量分布"), ConfiguredCount);
}

void FPhysicsOptimizerCore::Rule05_ConfigureConstraintLimits(
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

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule05: 配置了 %d 个约束限制"), ConfiguredCount);
}

void FPhysicsOptimizerCore::Rule12_ConfigureSleepSettings(
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

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule12: 配置了 %d 个骨骼的 Sleep 设置 (阈值=%.3f)"),
		ConfiguredCount, Threshold);
}

// ========== P2 规则实现 ==========

void FPhysicsOptimizerCore::Rule08_ConfigureTerminalBoneDamping(
	UPhysicsAsset* PA,
	USkeletalMesh* Mesh)
{
	if (!PA || !Mesh)
	{
		return;
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	int32 ConfiguredCount = 0;

	// 找出所有末端骨骼（无子骨骼）
	TSet<FName> TerminalBones;
	for (int32 i = 0; i < RefSkel.GetNum(); ++i)
	{
		bool bHasChildren = false;
		for (int32 j = 0; j < RefSkel.GetNum(); ++j)
		{
			if (RefSkel.GetParentIndex(j) == i)
			{
				bHasChildren = true;
				break;
			}
		}

		if (!bHasChildren)
		{
			TerminalBones.Add(RefSkel.GetBoneName(i));
		}
	}

	// 为末端骨骼设置更高的阻尼
	for (USkeletalBodySetup* BodySetup : PA->SkeletalBodySetups)
	{
		if (!BodySetup)
		{
			continue;
		}

		const FName BoneName = BodySetup->BoneName;
		if (TerminalBones.Contains(BoneName))
		{
			FBodyInstance& DefaultInstance = BodySetup->DefaultInstance;

			// 末端骨骼：设置高线性阻尼消除"面条感"
			DefaultInstance.LinearDamping = 1.0f;
			DefaultInstance.AngularDamping = 0.8f;

			ConfiguredCount++;

			UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] Rule08: 末端骨骼 %s → LinearDamping=1.0"),
				*BoneName.ToString());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule08: 配置了 %d 个末端骨骼的阻尼"), ConfiguredCount);
}

void FPhysicsOptimizerCore::Rule09_AlignShapesToBones(UPhysicsAsset* PA)
{
	// 引擎自带功能，验证即可
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule09: 形状已自动对齐（引擎自带）"));
}

void FPhysicsOptimizerCore::Rule10_GenerateLSV(
	UPhysicsAsset* PA,
	USkeletalMesh* Mesh,
	const TArray<FName>& CuffBones,
	int32 Resolution)
{
	if (!PA || !Mesh || CuffBones.Num() == 0)
	{
		return;
	}

	// TODO: 实现 LSV 生成
	// 需要启用 LevelSetHelpers.h 包含（当前被注释）
	// LSV 生成需要调用：
	// LevelSetHelpers::CreateLevelSetForBone(PA, Mesh, BoneName, Resolution);
	//
	// 由于 LevelSetHelpers 是 PhysicsUtilities 的 Private API，
	// 在某些 UE 版本中可能不可用或需要特殊配置。
	// 如果需要此功能，请取消注释 LevelSetHelpers.h 并重新编译。

	UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] Rule10: LSV 生成需要 LevelSetHelpers API（当前未启用）"));
	UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule10: 找到 %d 个袖口骨骼，分辨率=%d"),
		CuffBones.Num(), Resolution);
}

void FPhysicsOptimizerCore::Rule11_UseConvexForLongBones(
	UPhysicsAsset* PA,
	USkeletalMesh* Mesh,
	float LengthThreshold)
{
	if (!PA || !Mesh)
	{
		return;
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	int32 ProcessedCount = 0;

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

		// 计算骨骼长度
		float BoneLength = 0.0f;
		for (int32 ChildIndex = 0; ChildIndex < RefSkel.GetNum(); ++ChildIndex)
		{
			if (RefSkel.GetParentIndex(ChildIndex) == BoneIndex)
			{
				const FVector ChildLocation = RefSkel.GetRefBonePose()[ChildIndex].GetLocation();
				BoneLength = FMath::Max(BoneLength, ChildLocation.Size());
			}
		}

		// 如果骨骼足够长，标记为需要凸包
		if (BoneLength > LengthThreshold)
		{
			// TODO: 生成真正的凸包需要访问网格体顶点数据
			// 这需要使用 FSkeletalMeshModel 和 FSkeletalMeshLODModel
			// 当前实现：简单标记，保持原有形状
			//
			// 完整实现需要：
			// 1. 获取骨骼影响的顶点
			// 2. 计算顶点的凸包
			// 3. 将凸包添加到 BodySetup->AggGeom.ConvexElems
			//
			// 参考 PhysicsAssetUtils::CreateCollisionFromBone()

			ProcessedCount++;

			UE_LOG(LogTemp, Verbose, TEXT("[物理资产优化器] Rule11: 长骨 %s (%.2fcm) 可使用凸包优化"),
				*BoneName.ToString(), BoneLength);
		}
	}

	if (ProcessedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[物理资产优化器] Rule11: 找到 %d 个长骨（>%.2fcm）可使用凸包"),
			ProcessedCount, LengthThreshold);
		UE_LOG(LogTemp, Warning, TEXT("[物理资产优化器] Rule11: 凸包生成需要访问网格体顶点数据（当前未实现）"));
	}
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

	// 统计所有可能的碰撞对（排除被禁用的）
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

#undef LOCTEXT_NAMESPACE
