// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "ECFInstanceId.h"

std::atomic<uint64> FECFInstanceId::DynamicIdCounter{0};

FECFInstanceId FECFInstanceId::NewId()
{
	// 原子递增，确保多线程安全；fetch_add 返回旧值，+1 得到新ID
	const uint64 NewCounter = DynamicIdCounter.fetch_add(1, std::memory_order_relaxed) + 1;
	return FECFInstanceId(NewCounter);
}
