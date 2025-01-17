/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
#pragma once

#include "MinIndexQueue.h"
#include <cmath>
#include <iterator>
#include <vector>
#include <algorithm>

namespace RandomShuffles {

// 均匀分布的随机采样实现
template<typename It, typename Out, typename Rand>
Out UniformRandomSample(It begin, It end, Out out, std::size_t count, Rand randFunc) {
    using std::distance;
    auto sampleSize = static_cast<std::size_t>(std::distance(begin, end));
    
    if(sampleSize == 0 || count == 0) {
        return out;
    }

    // 创建索引数组
    std::vector<size_t> indices;
    indices.reserve(sampleSize);
    for(size_t i = 0; i < sampleSize; ++i) {
        indices.push_back(i);
    }

    // 进行count次采样
    for(size_t i = 0; i < count; ++i) {
        // 生成随机索引
        float r = randFunc(0.0f, 1.0f);
        size_t selectedIdx = static_cast<size_t>(r * sampleSize);
        if(selectedIdx >= sampleSize) {
            selectedIdx = sampleSize - 1;
        }
        
        // 输出选中的元素
        *out++ = *std::next(begin, indices[selectedIdx]);
    }
    
    return out;
}

template<typename It, typename Wt, typename Out, typename Rand>
Out RandomSample(It begin, It end, Wt weightBegin, Out out, std::size_t count, Rand randFunc) {
    using std::distance;
    auto sampleSize = static_cast<std::size_t>(std::distance(begin, end));
    
    // 首先保存所有权重
    std::vector<float> weights;
    weights.reserve(sampleSize);
    
    // 第一遍：保存权重并检查是否均匀分布
    bool isUniform = true;
    float firstWeight = -1.0f;
    
    for(size_t idx = 0; idx < sampleSize; ++idx) {
        float weight = static_cast<float>(*weightBegin++);
        weights.push_back(weight);
        
        if(idx == 0) {
            firstWeight = weight;
        } else if(weight != firstWeight) {
            isUniform = false;
        }
    }
    
    // 如果是均匀分布且权重大于0，使用UniformRandomSample
    if(isUniform && firstWeight > 0.0f) {
        return UniformRandomSample(begin, end, out, count, randFunc);
    }
    
    // 计算有效元素数量
    size_t validCount = 0;
    for(float weight : weights) {
        if(weight > 0.0f) {
            validCount++;
        }
    }

    // 如果没有有效元素或不需要采样，直接返回
    if (validCount == 0 || count == 0) {
        return out;
    }

    MinIndexQueue H(validCount);

    // 将所有有效元素推入优先队列
    for(size_t idx = 0; idx < sampleSize; ++idx) {
        float weight = weights[idx];
        if (weight > 0.0f) {
            float U = randFunc(0.0f, 1.0f);
            float R = std::pow(U, 1.0f/weight);
            H.Push(R, idx);
        }
    }

    // 进行count次采样
    std::vector<size_t> indices;
    indices.reserve(count);
    
    for(size_t sampleIdx = 0; sampleIdx < count; ++sampleIdx) {
        size_t selectedIndex = H.ExtractMin();
        indices.push_back(selectedIndex);
        
        // 重新计算该索引的R值并放回队列
        float weight = weights[selectedIndex];
        float U = randFunc(0.0f, 1.0f);
        float R = std::pow(U, 1.0f/weight);
        H.Push(R, selectedIndex);
    }

    // 输出结果
    for(size_t idx : indices) {
        *out++ = *std::next(begin, idx);
    }
    
    return out;
}
}