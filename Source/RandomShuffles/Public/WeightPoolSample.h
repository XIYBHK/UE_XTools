#pragma once

#include "CoreMinimal.h"

namespace RandomShuffles {

template<typename It, typename Wt, typename Out, typename Rand>
Out WeightPoolSample(It begin, It end, Wt weightBegin, Out out, int32 count, Rand randFunc) {
    // ✅ 使用UE兼容的迭代器计算方式
    int32 sampleSize = 0;
    for (It it = begin; it != end; ++it) {
        ++sampleSize;
    }

    // ✅ 使用UE容器替代STL - 计算权重总和并存储权重
    TArray<float> weights;
    weights.Reserve(sampleSize);
    float totalWeight = 0.0f;
    
    // 保存权重并计算总和
    for(int32 idx = 0; idx < sampleSize; ++idx) {
        float weight = static_cast<float>(*weightBegin++);
        weights.Add(weight);  // ✅ 使用UE容器方法
        if(weight > 0.0f) {
            totalWeight += weight;
        }
    }
    
    if(totalWeight <= 0.0f || count == 0) {
        return out;
    }

    // ✅ 使用UE容器 - 计算每个元素应该出现的次数
    TArray<int32> expectedCounts;
    expectedCounts.Reserve(sampleSize);
    int32 totalCount = 0;

    for(int32 idx = 0; idx < sampleSize; ++idx) {
        float weight = weights[idx];
        // 计算期望的出现次数，四舍五入
        int32 expectedCount = static_cast<int32>((weight / totalWeight) * count + 0.5f);
        expectedCounts.Add(expectedCount);  // ✅ 使用UE容器方法
        totalCount += expectedCount;
    }

    // ✅ 使用UE算法 - 调整总数以匹配要求的count
    while(totalCount > count) {
        // 找到最大的非零计数并减1
        int32 maxIdx = 0;
        int32 maxValue = expectedCounts[0];
        for(int32 i = 1; i < expectedCounts.Num(); ++i) {
            if(expectedCounts[i] > maxValue) {
                maxValue = expectedCounts[i];
                maxIdx = i;
            }
        }
        if(maxValue > 0) {
            expectedCounts[maxIdx]--;
            totalCount--;
        } else {
            break; // 防止无限循环
        }
    }

    // 预先找到权重最大的元素索引，避免重复查找
    int32 maxWeightIdx = 0;
    if(totalCount < count) {
        float maxWeight = weights[0];
        for(int32 i = 1; i < sampleSize; ++i) {
            if(weights[i] > maxWeight) {
                maxWeight = weights[i];
                maxWeightIdx = i;
            }
        }
    }

    while(totalCount < count) {
        expectedCounts[maxWeightIdx]++;
        totalCount++;
    }

    // ✅ 使用UE容器 - 创建结果数组
    TArray<int32> resultIndices;
    resultIndices.Reserve(count);

    // 按照计算的次数添加索引
    for(int32 idx = 0; idx < sampleSize; ++idx) {
        for(int32 j = 0; j < expectedCounts[idx]; ++j) {
            resultIndices.Add(idx);  // ✅ 使用UE容器方法
        }
    }

    // ✅ 使用UE算法 - 打乱结果数组
    for(int32 i = resultIndices.Num() - 1; i > 0; --i) {
        float r = randFunc(0.0f, 1.0f);
        int32 j = static_cast<int32>(r * (i + 1));
        if(j > i) j = i;
        resultIndices.Swap(i, j);  // ✅ 使用UE容器方法
    }

    // 输出结果
    for(int32 idx : resultIndices) {
        *out++ = *std::next(begin, idx);
    }
    
    return out;
}

} 