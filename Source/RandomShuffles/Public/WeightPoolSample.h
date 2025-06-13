#pragma once

#include <vector>
#include <algorithm>

namespace RandomShuffles {

template<typename It, typename Wt, typename Out, typename Rand>
Out WeightPoolSample(It begin, It end, Wt weightBegin, Out out, std::size_t count, Rand randFunc) {
    using std::distance;
    auto sampleSize = static_cast<std::size_t>(std::distance(begin, end));
    
    // 计算权重总和并存储权重
    std::vector<float> weights;
    weights.reserve(sampleSize);
    float totalWeight = 0.0f;
    
    // 保存权重并计算总和
    for(size_t idx = 0; idx < sampleSize; ++idx) {
        float weight = static_cast<float>(*weightBegin++);
        weights.push_back(weight);
        if(weight > 0.0f) {
            totalWeight += weight;
        }
    }
    
    if(totalWeight <= 0.0f || count == 0) {
        return out;
    }

    // 计算每个元素应该出现的次数
    std::vector<size_t> expectedCounts;
    expectedCounts.reserve(sampleSize);
    size_t totalCount = 0;

    for(size_t idx = 0; idx < sampleSize; ++idx) {
        float weight = weights[idx];
        // 计算期望的出现次数，四舍五入
        size_t expectedCount = static_cast<size_t>((weight / totalWeight) * count + 0.5f);
        expectedCounts.push_back(expectedCount);
        totalCount += expectedCount;
    }

    // 调整总数以匹配要求的count - 优化性能
    while(totalCount > count) {
        // 找到最大的非零计数并减1
        auto maxIt = std::max_element(expectedCounts.begin(), expectedCounts.end());
        if(*maxIt > 0) {
            (*maxIt)--;
            totalCount--;
        } else {
            break; // 防止无限循环
        }
    }

    // 预先找到权重最大的元素索引，避免重复查找
    size_t maxWeightIdx = 0;
    if(totalCount < count) {
        float maxWeight = weights[0];
        for(size_t i = 1; i < sampleSize; ++i) {
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

    // 创建结果数组
    std::vector<size_t> resultIndices;
    resultIndices.reserve(count);
    
    // 按照计算的次数添加索引
    for(size_t idx = 0; idx < sampleSize; ++idx) {
        for(size_t j = 0; j < expectedCounts[idx]; ++j) {
            resultIndices.push_back(idx);
        }
    }

    // 打乱结果数组
    for(size_t i = resultIndices.size() - 1; i > 0; --i) {
        float r = randFunc(0.0f, 1.0f);
        size_t j = static_cast<size_t>(r * (i + 1));
        if(j > i) j = i;
        std::swap(resultIndices[i], resultIndices[j]);
    }

    // 输出结果
    for(size_t idx : resultIndices) {
        *out++ = *std::next(begin, idx);
    }
    
    return out;
}

} 