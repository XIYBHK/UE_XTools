/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
#pragma once

#include <queue>
#include <vector>

namespace RandomShuffles {

class MinIndexQueue {
public:
	MinIndexQueue(std::size_t MaxSize); // 构造函数，初始化最大大小
	float MinimumKey() const; // Get the minimum key value
	std::size_t ExtractMin(); // 提取最小值
	void Push(float Priority, std::size_t Index); // 推入优先级和索引
	std::size_t Size() const; // 获取队列大小

private:
	struct Compare {
		const MinIndexQueue* Queue;

		bool operator()(std::size_t left, std::size_t right) const; // 比较两个索引的优先级
	};

	std::vector<float> Priorities; // 存储优先级的向量
	std::priority_queue<std::size_t, std::vector<std::size_t>, Compare> Queue; // 优先队列
};

}