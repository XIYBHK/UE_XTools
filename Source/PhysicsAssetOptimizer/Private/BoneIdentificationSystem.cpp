/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "BoneIdentificationSystem.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"

#define LOCTEXT_NAMESPACE "BoneIdentificationSystem"

TMap<FName, EBoneType> FBoneIdentificationSystem::IdentifyBones(USkeletalMesh* Mesh)
{
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[骨骼识别] 无效的骨骼网格体"));
		return TMap<FName, EBoneType>();
	}

	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	TMap<FName, EBoneType> BoneTypes;

	UE_LOG(LogTemp, Log, TEXT("[骨骼识别] 开始识别 %s (骨骼数: %d)"),
		*Mesh->GetName(), RefSkel.GetNum());

	// ========== 通道1: 命名规则（快速路径）==========
	UE_LOG(LogTemp, Verbose, TEXT("[骨骼识别] 通道1: 命名规则识别..."));
	TMap<FName, EBoneType> NameBasedTypes = IdentifyByNaming(RefSkel);

	// 计算命名识别覆盖率
	const float NameCoverageRatio = static_cast<float>(NameBasedTypes.Num()) / RefSkel.GetNum();
	UE_LOG(LogTemp, Log, TEXT("[骨骼识别] 命名识别覆盖率: %.1f%% (%d/%d)"),
		NameCoverageRatio * 100.0f, NameBasedTypes.Num(), RefSkel.GetNum());

	// 如果命名识别覆盖率≥80%，直接使用
	if (NameCoverageRatio >= 0.8f)
	{
		UE_LOG(LogTemp, Log, TEXT("[骨骼识别] 命名规则识别成功，跳过拓扑分析"));
		return NameBasedTypes;
	}

	// ========== 通道2: 拓扑链分析（核心）==========
	UE_LOG(LogTemp, Verbose, TEXT("[骨骼识别] 通道2: 拓扑链分析..."));
	TMap<FName, EBoneType> TopoBasedTypes = IdentifyByTopology(RefSkel);

	// 合并结果（命名规则优先）
	BoneTypes = TopoBasedTypes;
	for (const auto& Pair : NameBasedTypes)
	{
		BoneTypes.Add(Pair.Key, Pair.Value);
	}

	// ========== 通道3: 几何校验（纠偏）==========
	UE_LOG(LogTemp, Verbose, TEXT("[骨骼识别] 通道3: 几何校验..."));
	ValidateWithGeometry(Mesh, BoneTypes);

	UE_LOG(LogTemp, Log, TEXT("[骨骼识别] 识别完成，已识别 %d/%d 个骨骼"),
		BoneTypes.Num(), RefSkel.GetNum());

	return BoneTypes;
}

TArray<FName> FBoneIdentificationSystem::FindBonesByType(
	const TMap<FName, EBoneType>& BoneTypes,
	EBoneType Type)
{
	TArray<FName> Result;
	for (const auto& Pair : BoneTypes)
	{
		if (Pair.Value == Type)
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

// ========== 通道1: 命名规则 ==========

TMap<FName, EBoneType> FBoneIdentificationSystem::IdentifyByNaming(const FReferenceSkeleton& Skel)
{
	TMap<FName, EBoneType> Result;

	for (int32 i = 0; i < Skel.GetNum(); ++i)
	{
		const FName BoneName = Skel.GetBoneName(i);
		const FString BoneNameStr = BoneName.ToString().ToLower();

		// 按优先级检查骨骼类型
		if (MatchesBoneType(BoneNameStr, EBoneType::Head))
		{
			Result.Add(BoneName, EBoneType::Head);
		}
		else if (MatchesBoneType(BoneNameStr, EBoneType::Spine))
		{
			Result.Add(BoneName, EBoneType::Spine);
		}
		else if (MatchesBoneType(BoneNameStr, EBoneType::Clavicle))
		{
			Result.Add(BoneName, EBoneType::Clavicle);
		}
		else if (MatchesBoneType(BoneNameStr, EBoneType::Pelvis))
		{
			Result.Add(BoneName, EBoneType::Pelvis);
		}
		else if (MatchesBoneType(BoneNameStr, EBoneType::Hand))
		{
			Result.Add(BoneName, EBoneType::Hand);
		}
		else if (MatchesBoneType(BoneNameStr, EBoneType::Foot))
		{
			Result.Add(BoneName, EBoneType::Foot);
		}
		else if (MatchesBoneType(BoneNameStr, EBoneType::Finger))
		{
			Result.Add(BoneName, EBoneType::Finger);
		}
		else if (MatchesBoneType(BoneNameStr, EBoneType::Tail))
		{
			Result.Add(BoneName, EBoneType::Tail);
		}
		else if (MatchesBoneType(BoneNameStr, EBoneType::Face))
		{
			Result.Add(BoneName, EBoneType::Face);
		}
		// 手臂和腿部需要更复杂的匹配，先跳过
	}

	return Result;
}

bool FBoneIdentificationSystem::MatchesBoneType(const FString& BoneName, EBoneType Type)
{
	switch (Type)
	{
	case EBoneType::Spine:
		return BoneName.Contains(TEXT("spine")) || BoneName.Contains(TEXT("spn")) ||
		       BoneName.Contains(TEXT("back")) || BoneName.Contains(TEXT("脊")) ||
		       BoneName.Contains(TEXT("背"));

	case EBoneType::Head:
		return BoneName.Contains(TEXT("head")) || BoneName.Contains(TEXT("hd")) ||
		       BoneName.Contains(TEXT("skull")) || BoneName.Contains(TEXT("头")) ||
		       BoneName.Contains(TEXT("首"));

	case EBoneType::Clavicle:
		return BoneName.Contains(TEXT("clavicle")) || BoneName.Contains(TEXT("clav")) ||
		       BoneName.Contains(TEXT("collar")) || BoneName.Contains(TEXT("锁骨")) ||
		       BoneName.Contains(TEXT("肩.*骨根"));

	case EBoneType::Pelvis:
		return BoneName.Contains(TEXT("pelvis")) || BoneName.Contains(TEXT("hip")) ||
		       BoneName.Contains(TEXT("骨盆")) || BoneName.Contains(TEXT("腰"));

	case EBoneType::Hand:
		return (BoneName.Contains(TEXT("hand")) || BoneName.Contains(TEXT("wrist")) ||
		        BoneName.Contains(TEXT("手掌")) || BoneName.Contains(TEXT("腕"))) &&
		       !BoneName.Contains(TEXT("finger"));  // 排除手指

	case EBoneType::Foot:
		return (BoneName.Contains(TEXT("foot")) || BoneName.Contains(TEXT("ankle")) ||
		        BoneName.Contains(TEXT("脚")) || BoneName.Contains(TEXT("踝"))) &&
		       !BoneName.Contains(TEXT("toe"));  // 排除脚趾

	case EBoneType::Finger:
		return BoneName.Contains(TEXT("finger")) || BoneName.Contains(TEXT("thumb")) ||
		       BoneName.Contains(TEXT("index")) || BoneName.Contains(TEXT("middle")) ||
		       BoneName.Contains(TEXT("ring")) || BoneName.Contains(TEXT("pinky")) ||
		       BoneName.Contains(TEXT("手指"));

	case EBoneType::Tail:
		return BoneName.Contains(TEXT("tail")) || BoneName.Contains(TEXT("尾")) ||
		       BoneName.Contains(TEXT("tailbone")) || BoneName.Contains(TEXT("coccyx"));

	case EBoneType::Face:
		return BoneName.Contains(TEXT("face")) || BoneName.Contains(TEXT("jaw")) ||
		       BoneName.Contains(TEXT("eye")) || BoneName.Contains(TEXT("brow")) ||
		       BoneName.Contains(TEXT("lip")) || BoneName.Contains(TEXT("cheek")) ||
		       BoneName.Contains(TEXT("脸")) || BoneName.Contains(TEXT("面"));

	default:
		return false;
	}
}

// ========== 通道2: 拓扑链分析 ==========

TMap<FName, EBoneType> FBoneIdentificationSystem::IdentifyByTopology(const FReferenceSkeleton& Skel)
{
	TMap<FName, EBoneType> Result;

	if (Skel.GetNum() == 0)
	{
		return Result;
	}

	// 1. 分析骨骼树结构
	TMap<int32, int32> Depths;
	TMap<int32, int32> ChildCounts;
	AnalyzeBoneTree(Skel, 0, Depths, ChildCounts);

	// 2. 查找最长连续父子链（脊柱）
	TArray<int32> SpineChain;
	FindLongestChain(Skel, 0, SpineChain);

	// 将脊柱链标记为 Spine
	for (int32 BoneIndex : SpineChain)
	{
		Result.Add(Skel.GetBoneName(BoneIndex), EBoneType::Spine);
	}

	// 3. 头部：脊柱末端（最后一个骨骼）
	if (SpineChain.Num() > 0)
	{
		const int32 HeadBoneIndex = SpineChain.Last();
		Result.Add(Skel.GetBoneName(HeadBoneIndex), EBoneType::Head);
		UE_LOG(LogTemp, Verbose, TEXT("[骨骼识别] 头部: %s"), *Skel.GetBoneName(HeadBoneIndex).ToString());
	}

	// 4. 识别四肢链
	TArray<TArray<int32>> LimbChains = IdentifyLimbChains(Skel, SpineChain);
	for (const TArray<int32>& LimbChain : LimbChains)
	{
		if (LimbChain.Num() > 0)
		{
			// 暂时标记为 Arm（后续通过几何或命名细化为 Leg）
			for (int32 BoneIndex : LimbChain)
			{
				Result.Add(Skel.GetBoneName(BoneIndex), EBoneType::Arm);
			}
		}
	}

	// TODO: 5. 尾巴识别（脊柱根反方向）
	// TODO: 6. 手脚识别（四肢末端）

	UE_LOG(LogTemp, Log, TEXT("[骨骼识别] 拓扑分析: 脊柱链=%d, 四肢链=%d"),
		SpineChain.Num(), LimbChains.Num());

	return Result;
}

int32 FBoneIdentificationSystem::FindLongestChain(
	const FReferenceSkeleton& Skel,
	int32 StartBoneIndex,
	TArray<int32>& OutChain)
{
	OutChain.Empty();

	// DFS 查找最长链
	TArray<int32> CurrentChain;
	TArray<int32> LongestChain;
	int32 MaxLength = 0;

	// 简化实现：从根骨骼开始，沿着子骨骼数量=1的路径前进
	int32 CurrentBone = StartBoneIndex;
	while (true)
	{
		CurrentChain.Add(CurrentBone);

		TArray<int32> Children = GetDirectChildren(Skel, CurrentBone);
		if (Children.Num() == 0)
		{
			// 末端骨骼
			break;
		}
		else if (Children.Num() == 1)
		{
			// 继续延伸链
			CurrentBone = Children[0];
		}
		else
		{
			// 分叉点：选择子树最深的继续
			int32 DeepestChild = Children[0];
			int32 MaxDepth = 0;

			for (int32 Child : Children)
			{
				// 简单计算：递归计数子节点数量作为深度
				TMap<int32, int32> TempDepths;
				TMap<int32, int32> TempCounts;
				AnalyzeBoneTree(Skel, Child, TempDepths, TempCounts);

				if (TempDepths.Num() > MaxDepth)
				{
					MaxDepth = TempDepths.Num();
					DeepestChild = Child;
				}
			}

			CurrentBone = DeepestChild;
		}

		// 防止无限循环
		if (CurrentChain.Num() > 1000)
		{
			UE_LOG(LogTemp, Warning, TEXT("[骨骼识别] 链过长，可能存在循环引用"));
			break;
		}
	}

	OutChain = CurrentChain;
	return CurrentChain.Num();
}

void FBoneIdentificationSystem::AnalyzeBoneTree(
	const FReferenceSkeleton& Skel,
	int32 BoneIndex,
	TMap<int32, int32>& OutDepths,
	TMap<int32, int32>& OutChildCounts)
{
	OutDepths.Add(BoneIndex, 0);

	TArray<int32> Children = GetDirectChildren(Skel, BoneIndex);
	OutChildCounts.Add(BoneIndex, Children.Num());

	for (int32 Child : Children)
	{
		AnalyzeBoneTree(Skel, Child, OutDepths, OutChildCounts);
	}
}

TArray<TArray<int32>> FBoneIdentificationSystem::IdentifyLimbChains(
	const FReferenceSkeleton& Skel,
	const TArray<int32>& SpineChain)
{
	TArray<TArray<int32>> LimbChains;

	// 从脊柱的每个骨骼查找分叉点
	for (int32 SpineBone : SpineChain)
	{
		TArray<int32> Children = GetDirectChildren(Skel, SpineBone);

		for (int32 Child : Children)
		{
			// 如果子骨骼不在脊柱链中，则可能是四肢
			if (!SpineChain.Contains(Child))
			{
				TArray<int32> LimbChain;
				FindLongestChain(Skel, Child, LimbChain);

				// 只添加足够长的链（至少3个骨骼）
				if (LimbChain.Num() >= 3)
				{
					LimbChains.Add(LimbChain);
				}
			}
		}
	}

	return LimbChains;
}

// ========== 通道3: 几何校验 ==========

void FBoneIdentificationSystem::ValidateWithGeometry(
	USkeletalMesh* Mesh,
	TMap<FName, EBoneType>& InOutBoneTypes)
{
	// TODO: 实现几何校验
	// - 检查头部包围盒宽高比
	// - 检查袖口/领口法向突变
	// - 检查尾巴方向向量

	UE_LOG(LogTemp, Verbose, TEXT("[骨骼识别] 几何校验（待实现）"));
}

bool FBoneIdentificationSystem::IsHeadBone(USkeletalMesh* Mesh, int32 BoneIndex)
{
	// TODO: 计算顶点包围盒，检查宽高比是否接近1（0.8~1.2）
	return false;
}

bool FBoneIdentificationSystem::IsCuffBone(USkeletalMesh* Mesh, int32 BoneIndex)
{
	// TODO: 检查骨骼长度 < 5cm 且法向突变 > 60°
	return false;
}

// ========== 辅助函数 ==========

float FBoneIdentificationSystem::CalculateBoneLength(
	const FReferenceSkeleton& Skel,
	int32 BoneIndex)
{
	TArray<int32> Children = GetDirectChildren(Skel, BoneIndex);
	if (Children.Num() == 0)
	{
		return 0.0f;
	}

	// 计算到最远子骨骼的距离
	float MaxLength = 0.0f;
	const FTransform& BoneTransform = Skel.GetRefBonePose()[BoneIndex];

	for (int32 Child : Children)
	{
		const FTransform& ChildTransform = Skel.GetRefBonePose()[Child];
		const FVector Offset = ChildTransform.GetLocation() - BoneTransform.GetLocation();
		MaxLength = FMath::Max(MaxLength, Offset.Size());
	}

	return MaxLength;
}

TArray<int32> FBoneIdentificationSystem::GetDirectChildren(
	const FReferenceSkeleton& Skel,
	int32 BoneIndex)
{
	TArray<int32> Children;
	for (int32 i = 0; i < Skel.GetNum(); ++i)
	{
		if (Skel.GetParentIndex(i) == BoneIndex)
		{
			Children.Add(i);
		}
	}
	return Children;
}

bool FBoneIdentificationSystem::IsTerminalBone(
	const FReferenceSkeleton& Skel,
	int32 BoneIndex)
{
	return GetDirectChildren(Skel, BoneIndex).Num() == 0;
}

#undef LOCTEXT_NAMESPACE
