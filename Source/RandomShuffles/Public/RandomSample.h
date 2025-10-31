/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
#pragma once

// 只使用UE核心头文件
#include "CoreMinimal.h"
#include "MinIndexQueue.h"

namespace RandomShuffles {

// 使用UE容器 - 均匀分布的随机采样实现
template<typename It, typename Out, typename Rand>
Out UniformRandomSample(It begin, It end, Out out, int32 count, Rand randFunc) {
    // 使用UE兼容的迭代器计算方式
    int32 sampleSize = 0;
    for (It it = begin; it != end; ++it) {
        ++sampleSize;
    }

    if(sampleSize == 0 || count == 0) {
        return out;
    }

    // 使用UE容器 - 创建索引数组
    TArray<int32> indices;
    indices.Reserve(sampleSize);
    for(int32 i = 0; i < sampleSize; ++i) {
        indices.Add(i);  // 使用UE容器方法
    }

    // 进行count次采样
    for(int32 i = 0; i < count; ++i) {
        // 生成随机索引
        float r = randFunc(0.0f, 1.0f);
        int32 selectedIdx = static_cast<int32>(r * sampleSize);
        if(selectedIdx >= sampleSize) {
            selectedIdx = sampleSize - 1;
        }

        // 使用UE算法 - 输出选中的元素
        *out++ = *(begin + indices[selectedIdx]);
    }
    
    return out;
}

// 使用UE类型和容器 - 加权随机采样实现
template<typename It, typename Wt, typename Out, typename Rand>
Out RandomSample(It begin, It end, Wt weightBegin, Out out, int32 count, Rand randFunc) {
    // 使用UE兼容的迭代器计算方式
    int32 sampleSize = 0;
    for (It it = begin; it != end; ++it) {
        ++sampleSize;
    }
    
    // 使用UE容器 - 保存所有权重
    TArray<float> weights;
    weights.Reserve(sampleSize);
    
    // 第一遍：保存权重并检查是否均匀分布
    bool isUniform = true;
    float firstWeight = -1.0f;
    
    for(int32 idx = 0; idx < sampleSize; ++idx) {
        float weight = static_cast<float>(*weightBegin++);
        weights.Add(weight);  // 使用UE容器方法
        
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
    int32 validCount = 0;
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
    for(int32 idx = 0; idx < sampleSize; ++idx) {
        float weight = weights[idx];
        if (weight > 0.0f) {
            float U = randFunc(0.0f, 1.0f);
            float R = FMath::Pow(U, 1.0f/weight);  // 使用UE数学函数
            H.Push(R, idx);
        }
    }

    // 使用UE容器 - 进行count次采样
    TArray<int32> indices;
    indices.Reserve(count);
    
    for(int32 sampleIdx = 0; sampleIdx < count; ++sampleIdx) {
        int32 selectedIndex = H.ExtractMin();
        indices.Add(selectedIndex);  // 使用UE容器方法
        
        // 重新计算该索引的R值并放回队列
        float weight = weights[selectedIndex];
        float U = randFunc(0.0f, 1.0f);
        float R = FMath::Pow(U, 1.0f/weight);  // 使用UE数学函数
        H.Push(R, selectedIndex);
    }

    // 输出结果
    for(int32 idx : indices) {
        *out++ = *(begin + idx);
    }
    
    return out;
}
}