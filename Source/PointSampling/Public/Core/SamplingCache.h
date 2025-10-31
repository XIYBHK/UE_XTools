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
	FVector BoxExtent;
	FVector Position;            // 世界空间位置（仅World空间使用）
	FQuat Rotation;
	float Radius;
	int32 TargetPointCount;
	float JitterStrength;
	bool bIs2D;
	EPoissonCoordinateSpace CoordinateSpace;
	
	bool operator==(const FPoissonCacheKey& Other) const
	{
		// 基础参数比较
		bool bBasicMatch = BoxExtent.Equals(Other.BoxExtent, 0.1f) &&
			   Rotation.Equals(Other.Rotation, 0.001f) &&
			   FMath::IsNearlyEqual(Radius, Other.Radius, 0.1f) &&
			   TargetPointCount == Other.TargetPointCount &&
			   FMath::IsNearlyEqual(JitterStrength, Other.JitterStrength, 0.01f) &&
			   bIs2D == Other.bIs2D &&
			   CoordinateSpace == Other.CoordinateSpace;
		
		// 仅World空间比较Position（Local/Raw空间输出相对坐标，与Position无关）
		if (bBasicMatch && CoordinateSpace == EPoissonCoordinateSpace::World)
		{
			return Position.Equals(Other.Position, 0.1f);
		}
		
		return bBasicMatch;
	}
	
	friend uint32 GetTypeHash(const FPoissonCacheKey& Key)
	{
		// 基础Hash（所有空间共用）
		uint32 Hash = HashCombine(
			HashCombine(
				HashCombine(
					HashCombine(
						GetTypeHash(Key.BoxExtent),
						GetTypeHash(Key.Rotation)
					),
					GetTypeHash(Key.Radius)
				),
				GetTypeHash(Key.TargetPointCount)
			),
			GetTypeHash(Key.JitterStrength)
		);
		
		// 仅World空间包含Position的Hash
		if (Key.CoordinateSpace == EPoissonCoordinateSpace::World)
		{
			Hash = HashCombine(Hash, GetTypeHash(Key.Position));
		}
		
		// 添加bIs2D和CoordinateSpace
		Hash = HashCombine(Hash, 
			HashCombine(GetTypeHash(Key.bIs2D), static_cast<uint32>(Key.CoordinateSpace))
		);
		
		return Hash;
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
	
	/** 清空缓存 */
	void ClearCache();
	
	/** 获取缓存统计 */
	void GetStats(int32& OutHits, int32& OutMisses);
	
private:
	FSamplingCache() = default;
	
	/** 移除最久未使用的条目（LRU淘汰） */
	void RemoveLRUEntry();
	
	static constexpr int32 MaxCacheSize = 50;
	
	FCriticalSection CacheLock;
	TMap<FPoissonCacheKey, TArray<FVector>> Cache;
	TMap<FPoissonCacheKey, double> AccessTimes;  // 记录每个条目的最后访问时间
	int32 CacheHits = 0;
	int32 CacheMisses = 0;
};

