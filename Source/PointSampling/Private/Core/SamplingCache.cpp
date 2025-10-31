/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#include "Core/SamplingCache.h"
#include "HAL/PlatformTime.h"
#include "PointSamplingTypes.h"

TOptional<TArray<FVector>> FSamplingCache::GetCached(const FPoissonCacheKey& Key)
{
	FScopeLock Lock(&CacheLock);
	if (const TArray<FVector>* Found = Cache.Find(Key))
	{
		//  LRU策略：更新访问时间
		AccessTimes.Add(Key, FPlatformTime::Seconds());
		
		CacheHits++;
		return *Found;
	}
	CacheMisses++;
	return {};
}

void FSamplingCache::Store(const FPoissonCacheKey& Key, const TArray<FVector>& Points)
{
	FScopeLock Lock(&CacheLock);
	
	//  LRU淘汰策略：缓存满时移除最久未使用的条目
	if (Cache.Num() >= MaxCacheSize)
	{
		RemoveLRUEntry();
	}
	
	Cache.Add(Key, Points);
	AccessTimes.Add(Key, FPlatformTime::Seconds());
}

void FSamplingCache::ClearCache()
{
	FScopeLock Lock(&CacheLock);
	Cache.Empty();
	AccessTimes.Empty();
	CacheHits = 0;
	CacheMisses = 0;
}

void FSamplingCache::GetStats(int32& OutHits, int32& OutMisses)
{
	FScopeLock Lock(&CacheLock);
	OutHits = CacheHits;
	OutMisses = CacheMisses;
}

void FSamplingCache::RemoveLRUEntry()
{
	if (Cache.Num() == 0) return;
	
	// 找到最早访问的条目
	FPoissonCacheKey OldestKey;
	double OldestTime = TNumericLimits<double>::Max();
	
	for (const auto& Pair : AccessTimes)
	{
		if (Pair.Value < OldestTime)
		{
			OldestTime = Pair.Value;
			OldestKey = Pair.Key;
		}
	}
	
	// 删除最旧的条目
	Cache.Remove(OldestKey);
	AccessTimes.Remove(OldestKey);
	
	UE_LOG(LogPointSampling, Verbose, TEXT("泊松缓存: LRU淘汰一个条目，剩余 %d 个"), Cache.Num());
}
