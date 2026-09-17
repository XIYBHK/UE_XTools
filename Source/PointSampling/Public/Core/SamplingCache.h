/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

/**
 * 缓存键结构体
 */
struct FPoissonCacheKey
{
	FVector BoxExtent = FVector::ZeroVector;
	FVector Position = FVector::ZeroVector;            // 世界空间位置（仅World空间使用）
	FQuat Rotation = FQuat::Identity;
	FVector Scale = FVector::OneVector;                 // 父缩放（仅Local/Raw空间使用，用于缩放补偿）
	float Radius = 0.0f;
	int32 TargetPointCount = 0;
	int32 MaxAttempts = 0;
	float JitterStrength = 0.0f;
	bool bIs2D = false;
	EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::World;

private:
	static FQuat CanonicalizeRotation(FQuat RotationToCanonicalize)
	{
		// q 与 -q 表示同一旋转。W 为零时继续按 XYZ 选择唯一符号，覆盖精确 180° 旋转。
		const bool bShouldFlip = RotationToCanonicalize.W < 0.0f
			|| (RotationToCanonicalize.W == 0.0f && (RotationToCanonicalize.X < 0.0f
				|| (RotationToCanonicalize.X == 0.0f && (RotationToCanonicalize.Y < 0.0f
					|| (RotationToCanonicalize.Y == 0.0f && RotationToCanonicalize.Z < 0.0f)))));
		if (bShouldFlip)
		{
			RotationToCanonicalize.X = -RotationToCanonicalize.X;
			RotationToCanonicalize.Y = -RotationToCanonicalize.Y;
			RotationToCanonicalize.Z = -RotationToCanonicalize.Z;
			RotationToCanonicalize.W = -RotationToCanonicalize.W;
		}
		return RotationToCanonicalize;
	}

public:
	
	bool operator==(const FPoissonCacheKey& Other) const
	{
		// 缓存保存最终点位，必须使用生成时的原始参数，不合并近似输入。
		if (CoordinateSpace != Other.CoordinateSpace || BoxExtent != Other.BoxExtent
			|| Radius != Other.Radius || TargetPointCount != Other.TargetPointCount
			|| MaxAttempts != Other.MaxAttempts || JitterStrength != Other.JitterStrength
			|| bIs2D != Other.bIs2D)
		{
			return false;
		}
		if (CoordinateSpace == EPoissonCoordinateSpace::World)
		{
			return Position == Other.Position
				&& CanonicalizeRotation(Rotation) == CanonicalizeRotation(Other.Rotation);
		}
		return Scale == Other.Scale;
	}

	friend uint32 GetTypeHash(const FPoissonCacheKey& Key)
	{
		// 精确比较下 +0/-0 相等；保留 FVector/FQuat 的 double 精度。
		auto HashScalar = [](double Value) { return GetTypeHash(Value == 0.0 ? 0.0 : Value); };
		auto HashVector = [&HashScalar](const FVector& Value)
		{
			return HashCombine(HashCombine(HashScalar(Value.X), HashScalar(Value.Y)), HashScalar(Value.Z));
		};
		uint32 Hash = HashVector(Key.BoxExtent);
		Hash = HashCombine(Hash, HashScalar(Key.Radius));
		Hash = HashCombine(Hash, GetTypeHash(Key.TargetPointCount));
		Hash = HashCombine(Hash, GetTypeHash(Key.MaxAttempts));
		Hash = HashCombine(Hash, HashScalar(Key.JitterStrength));
		Hash = HashCombine(Hash, GetTypeHash(Key.bIs2D));
		Hash = HashCombine(Hash, static_cast<uint32>(Key.CoordinateSpace));
		if (Key.CoordinateSpace == EPoissonCoordinateSpace::World)
		{
			Hash = HashCombine(Hash, HashVector(Key.Position));
			const FQuat Q = CanonicalizeRotation(Key.Rotation);
			Hash = HashCombine(Hash, HashScalar(Q.X));
			Hash = HashCombine(Hash, HashScalar(Q.Y));
			Hash = HashCombine(Hash, HashScalar(Q.Z));
			Hash = HashCombine(Hash, HashScalar(Q.W));
		}
		else
		{
			Hash = HashCombine(Hash, HashVector(Key.Scale));
		}
		return Hash;
	}
};

/** 圆形/球体规则点阵的局部几何缓存键。 */
struct FCircleSamplingCacheKey
{
	int32 PointCount = 0;
	int32 RandomSeed = 0;
	int32 MinimumPointsPerRing = 0;
	float Radius = 0.0f;
	float MinDistance = 0.0f;
	float StartAngle = 0.0f;
	float JitterStrength = 0.0f;
	float RingSpacing = 0.0f;
	float LayerSpacing = 0.0f;
	float LayerDensity = 1.0f;
	uint8 DistributionMode = 0;
	bool bIs3D = false;
	bool bSolid = false;
	bool bClockwise = false;

	bool operator==(const FCircleSamplingCacheKey& Other) const
	{
		// 生成使用原始参数，缓存不得合并相近但不同的几何请求。
		return PointCount == Other.PointCount
			&& RandomSeed == Other.RandomSeed
			&& MinimumPointsPerRing == Other.MinimumPointsPerRing
			&& Radius == Other.Radius
			&& MinDistance == Other.MinDistance
			&& StartAngle == Other.StartAngle
			&& JitterStrength == Other.JitterStrength
			&& RingSpacing == Other.RingSpacing
			&& LayerSpacing == Other.LayerSpacing
			&& LayerDensity == Other.LayerDensity
			&& DistributionMode == Other.DistributionMode
			&& bIs3D == Other.bIs3D
			&& bSolid == Other.bSolid
			&& bClockwise == Other.bClockwise;
	}

	friend uint32 GetTypeHash(const FCircleSamplingCacheKey& Key)
	{
		// +0 与 -0 比较相等，哈希也必须一致。
		auto HashFloat = [](float Value) { return GetTypeHash(Value == 0.0f ? 0.0f : Value); };
		uint32 Hash = GetTypeHash(Key.PointCount);
		Hash = HashCombine(Hash, GetTypeHash(Key.RandomSeed));
		Hash = HashCombine(Hash, GetTypeHash(Key.MinimumPointsPerRing));
		Hash = HashCombine(Hash, HashFloat(Key.Radius));
		Hash = HashCombine(Hash, HashFloat(Key.MinDistance));
		Hash = HashCombine(Hash, HashFloat(Key.StartAngle));
		Hash = HashCombine(Hash, HashFloat(Key.JitterStrength));
		Hash = HashCombine(Hash, HashFloat(Key.RingSpacing));
		Hash = HashCombine(Hash, HashFloat(Key.LayerSpacing));
		Hash = HashCombine(Hash, HashFloat(Key.LayerDensity));
		Hash = HashCombine(Hash, GetTypeHash(Key.DistributionMode));
		Hash = HashCombine(Hash, GetTypeHash(Key.bIs3D));
		Hash = HashCombine(Hash, GetTypeHash(Key.bSolid));
		return HashCombine(Hash, GetTypeHash(Key.bClockwise));
	}
};

/**
 * 采样结果缓存系统
 * 
 * 使用LRU（最近最少使用）策略管理缓存，避免性能抖动
 */
class POINTSAMPLING_API FSamplingCache
{
public:
	/** 获取单例 */
	static FSamplingCache& Get()
	{
		static FSamplingCache Instance;
		return Instance;
	}
	
	/** 获取缓存结果 */
	TOptional<TArray<FVector>> GetCached(const FPoissonCacheKey& Key);
	
	/** 存储结果到缓存 */
	void Store(const FPoissonCacheKey& Key, const TArray<FVector>& Points);

	/** 获取圆形/球体规则点阵的局部几何缓存结果。 */
	TOptional<TArray<FVector>> GetCachedCircle(const FCircleSamplingCacheKey& Key);

	/** 存储圆形/球体规则点阵的局部几何缓存结果。 */
	void StoreCircle(const FCircleSamplingCacheKey& Key, const TArray<FVector>& Points);
	
	/** 清空缓存 */
	void ClearCache();
	
	/** 获取缓存统计 */
	void GetStats(int32& OutHits, int32& OutMisses);

	/** 获取圆形/球体规则点阵缓存统计。 */
	void GetCircleStats(int32& OutHits, int32& OutMisses);
	
private:
	FSamplingCache() = default;
	
	/** 移除最久未使用的条目（LRU淘汰） */
	void RemoveLRUEntry();
	void RemoveCircleLRUEntry();

	static constexpr int32 MaxCacheSize = 50;
	static constexpr int32 MaxCircleCacheSize = 50;
	
	FCriticalSection CacheLock;
	TMap<FPoissonCacheKey, TArray<FVector>> Cache;
	TMap<FPoissonCacheKey, double> AccessTimes;  // 记录每个条目的最后访问时间
	int32 CacheHits = 0;
	int32 CacheMisses = 0;
	TMap<FCircleSamplingCacheKey, TArray<FVector>> CircleCache;
	TMap<FCircleSamplingCacheKey, double> CircleAccessTimes;
	int32 CircleCacheHits = 0;
	int32 CircleCacheMisses = 0;
};

