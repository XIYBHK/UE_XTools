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
	FVector Scale;               // 父缩放（仅Local/Raw空间使用，用于缩放补偿）
	float Radius;
	int32 TargetPointCount;
	int32 MaxAttempts;
	float JitterStrength;
	bool bIs2D;
	EPoissonCoordinateSpace CoordinateSpace;
	
	bool operator==(const FPoissonCacheKey& Other) const
	{
		// 注意：TMap 的 Key 需要“等价关系”（自反/对称/传递）。
		// 之前使用“容差比较”会破坏传递性，进而导致 Hash/Equals 合约不成立。
		// 这里改为“量化后比较”，保证缓存键行为稳定且可预测。

		if (CoordinateSpace != Other.CoordinateSpace)
		{
			return false;
		}

		auto QuantizeFloat = [](float Value, float Step) -> int32
		{
			return FMath::RoundToInt(Value / Step);
		};

		auto QuantizeVector = [&](const FVector& Value, float Step) -> FIntVector
		{
			return FIntVector(
				QuantizeFloat(Value.X, Step),
				QuantizeFloat(Value.Y, Step),
				QuantizeFloat(Value.Z, Step)
			);
		};

		auto CanonicalQuat = [](FQuat Q) -> FQuat
		{
			// q 与 -q 表示同一旋转，归一到同一半球，避免缓存键抖动
			if (Q.W < 0.0f)
			{
				Q.X = -Q.X; Q.Y = -Q.Y; Q.Z = -Q.Z; Q.W = -Q.W;
			}
			return Q;
		};

		auto QuantizeQuat = [&](const FQuat& Value, float Step, FIntVector& OutXYZ, int32& OutW) -> void
		{
			const FQuat Q = CanonicalQuat(Value);
			OutXYZ = FIntVector(
				QuantizeFloat(Q.X, Step),
				QuantizeFloat(Q.Y, Step),
				QuantizeFloat(Q.Z, Step)
			);
			OutW = QuantizeFloat(Q.W, Step);
		};

		// 基础参数：全部空间共用
		if (QuantizeVector(BoxExtent, 0.1f) != QuantizeVector(Other.BoxExtent, 0.1f))
		{
			return false;
		}

		if (QuantizeFloat(Radius, 0.1f) != QuantizeFloat(Other.Radius, 0.1f))
		{
			return false;
		}

		if (TargetPointCount != Other.TargetPointCount)
		{
			return false;
		}

		if (MaxAttempts != Other.MaxAttempts)
		{
			return false;
		}

		if (QuantizeFloat(JitterStrength, 0.01f) != QuantizeFloat(Other.JitterStrength, 0.01f))
		{
			return false;
		}

		if (bIs2D != Other.bIs2D)
		{
			return false;
		}

		// World 空间：输出依赖 Position + Rotation（不依赖 Scale）
		if (CoordinateSpace == EPoissonCoordinateSpace::World)
		{
			if (QuantizeVector(Position, 0.1f) != QuantizeVector(Other.Position, 0.1f))
			{
				return false;
			}

			FIntVector ThisRotXYZ, OtherRotXYZ;
			int32 ThisRotW = 0, OtherRotW = 0;
			QuantizeQuat(Rotation, 0.001f, ThisRotXYZ, ThisRotW);
			QuantizeQuat(Other.Rotation, 0.001f, OtherRotXYZ, OtherRotW);
			return ThisRotXYZ == OtherRotXYZ && ThisRotW == OtherRotW;
		}

		// Local/Raw 空间：输出依赖 Scale 补偿（不依赖 Position/Rotation）
		return QuantizeVector(Scale, 0.001f) == QuantizeVector(Other.Scale, 0.001f);
	}
	
	friend uint32 GetTypeHash(const FPoissonCacheKey& Key)
	{
		auto QuantizeFloat = [](float Value, float Step) -> int32
		{
			return FMath::RoundToInt(Value / Step);
		};

		auto QuantizeVector = [&](const FVector& Value, float Step) -> FIntVector
		{
			return FIntVector(
				QuantizeFloat(Value.X, Step),
				QuantizeFloat(Value.Y, Step),
				QuantizeFloat(Value.Z, Step)
			);
		};

		auto CanonicalQuat = [](FQuat Q) -> FQuat
		{
			if (Q.W < 0.0f)
			{
				Q.X = -Q.X; Q.Y = -Q.Y; Q.Z = -Q.Z; Q.W = -Q.W;
			}
			return Q;
		};

		auto QuantizeQuat = [&](const FQuat& Value, float Step, FIntVector& OutXYZ, int32& OutW) -> void
		{
			const FQuat Q = CanonicalQuat(Value);
			OutXYZ = FIntVector(
				QuantizeFloat(Q.X, Step),
				QuantizeFloat(Q.Y, Step),
				QuantizeFloat(Q.Z, Step)
			);
			OutW = QuantizeFloat(Q.W, Step);
		};

		uint32 Hash = 0;

		auto HashIntVector = [](const FIntVector& V) -> uint32
		{
			uint32 H = 0;
			H = HashCombine(H, GetTypeHash(V.X));
			H = HashCombine(H, GetTypeHash(V.Y));
			H = HashCombine(H, GetTypeHash(V.Z));
			return H;
		};

		// 基础字段（所有空间共用）
		Hash = HashCombine(Hash, HashIntVector(QuantizeVector(Key.BoxExtent, 0.1f)));
		Hash = HashCombine(Hash, GetTypeHash(QuantizeFloat(Key.Radius, 0.1f)));
		Hash = HashCombine(Hash, GetTypeHash(Key.TargetPointCount));
		Hash = HashCombine(Hash, GetTypeHash(Key.MaxAttempts));
		Hash = HashCombine(Hash, GetTypeHash(QuantizeFloat(Key.JitterStrength, 0.01f)));
		Hash = HashCombine(Hash, GetTypeHash(Key.bIs2D));
		Hash = HashCombine(Hash, static_cast<uint32>(Key.CoordinateSpace));

		// World 空间字段
		if (Key.CoordinateSpace == EPoissonCoordinateSpace::World)
		{
			Hash = HashCombine(Hash, HashIntVector(QuantizeVector(Key.Position, 0.1f)));
			FIntVector RotXYZ;
			int32 RotW = 0;
			QuantizeQuat(Key.Rotation, 0.001f, RotXYZ, RotW);
			Hash = HashCombine(Hash, HashIntVector(RotXYZ));
			Hash = HashCombine(Hash, GetTypeHash(RotW));
		}
		else
		{
			// Local/Raw 空间字段
			Hash = HashCombine(Hash, HashIntVector(QuantizeVector(Key.Scale, 0.001f)));
		}

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

