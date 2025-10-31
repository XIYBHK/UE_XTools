/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "MinIndexQueue.h"

namespace RandomShuffles {

MinIndexQueue::MinIndexQueue(int32 MaxSize)
{
	// 使用 UE 容器预分配内存
	Priorities.SetNumZeroed(MaxSize);
	Heap.Reserve(MaxSize);
}

float MinIndexQueue::MinimumKey() const
{
	check(Heap.Num() > 0);
	return Priorities[Heap[0]];
}

int32 MinIndexQueue::ExtractMin()
{
	check(Heap.Num() > 0);

	const int32 MinIndex = Heap[0];

	// 将最后一个元素移到根节点
	Heap[0] = Heap.Last();
	
	// UE 5.5+ API 变更：Pop 参数从 bool 改为 EAllowShrinking 枚举
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	Heap.Pop(EAllowShrinking::No);
#else
	Heap.Pop(false);  // false = 不缩容
#endif

	// 如果还有元素，执行下沉操作
	if (Heap.Num() > 0)
	{
		HeapifyDown(0);
	}

	return MinIndex;
}

void MinIndexQueue::Push(float Priority, int32 Index)
{
	Priorities[Index] = Priority;

	// 将新索引添加到堆末尾
	Heap.Add(Index);

	// 执行上浮操作
	HeapifyUp(Heap.Num() - 1);
}

int32 MinIndexQueue::Size() const
{
	return Heap.Num();
}

void MinIndexQueue::HeapifyUp(int32 HeapIndex)
{
	while (HeapIndex > 0)
	{
		const int32 ParentIdx = ParentIndex(HeapIndex);

		// 如果当前节点的优先级小于父节点，则交换
		if (Priorities[Heap[HeapIndex]] < Priorities[Heap[ParentIdx]])
		{
			Heap.Swap(HeapIndex, ParentIdx);
			HeapIndex = ParentIdx;
		}
		else
		{
			break;
		}
	}
}

void MinIndexQueue::HeapifyDown(int32 HeapIndex)
{
	const int32 HeapSize = Heap.Num();

	while (true)
	{
		int32 SmallestIdx = HeapIndex;
		const int32 LeftIdx = LeftChildIndex(HeapIndex);
		const int32 RightIdx = RightChildIndex(HeapIndex);

		// 找到当前节点和子节点中优先级最小的
		if (LeftIdx < HeapSize && Priorities[Heap[LeftIdx]] < Priorities[Heap[SmallestIdx]])
		{
			SmallestIdx = LeftIdx;
		}

		if (RightIdx < HeapSize && Priorities[Heap[RightIdx]] < Priorities[Heap[SmallestIdx]])
		{
			SmallestIdx = RightIdx;
		}

		// 如果当前节点已经是最小的，则停止
		if (SmallestIdx == HeapIndex)
		{
			break;
		}

		// 否则交换并继续下沉
		Heap.Swap(HeapIndex, SmallestIdx);
		HeapIndex = SmallestIdx;
	}
}

}
