/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"

namespace RandomShuffles {

/**
 * 最小索引优先队列
 * 使用 UE TArray 实现的最小堆，替代 STL priority_queue
 */
class MinIndexQueue
{
public:
	/** 构造函数，初始化最大大小 */
	MinIndexQueue(int32 MaxSize);

	/** 获取最小优先级 */
	float MinimumKey() const;

	/** 提取最小值索引 */
	int32 ExtractMin();

	/** 推入优先级和索引 */
	void Push(float Priority, int32 Index);

	/** 获取队列大小 */
	int32 Size() const;

private:
	/** 存储每个索引的优先级 */
	TArray<float> Priorities;

	/** 最小堆存储索引 */
	TArray<int32> Heap;

	/** 上浮操作 */
	void HeapifyUp(int32 HeapIndex);

	/** 下沉操作 */
	void HeapifyDown(int32 HeapIndex);

	/** 获取父节点索引 */
	FORCEINLINE int32 ParentIndex(int32 ChildIndex) const
	{
		return (ChildIndex - 1) / 2;
	}

	/** 获取左子节点索引 */
	FORCEINLINE int32 LeftChildIndex(int32 ParentIndex) const
	{
		return 2 * ParentIndex + 1;
	}

	/** 获取右子节点索引 */
	FORCEINLINE int32 RightChildIndex(int32 ParentIndex) const
	{
		return 2 * ParentIndex + 2;
	}
};

}
