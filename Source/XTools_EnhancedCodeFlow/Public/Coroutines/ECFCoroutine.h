// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#ifdef __cpp_impl_coroutine

#include <coroutine>

/**
 * Defining coroutine handlers and promises in order to get coroutines work.
 */

struct FECFCoroutinePromise;
using FECFCoroutineHandle = std::coroutine_handle<FECFCoroutinePromise>;

struct FECFCoroutine : FECFCoroutineHandle
{
	using promise_type = ::FECFCoroutinePromise;
};

struct FECFCoroutinePromise
{
	FECFCoroutine get_return_object() { return { FECFCoroutine::from_promise(*this) }; }
	std::suspend_never initial_suspend() noexcept { return {}; }
	std::suspend_never final_suspend() noexcept { return {}; }
	void return_void() { bHasFinished = true; }
	void unhandled_exception()
	{
		// 标记协程已结束，避免悬空引用
		bHasFinished = true;
		bHasError = true;
		// 注意：UE 默认禁用异常，此方法通常不会被调用
		// 但为了防御性编程，仍然设置完成标志
	}
	bool bHasFinished = false;
	bool bHasError = false;
};

#else

/**
 * Create dummy implementations of coroutine handles if coroutines are not supported by a compiler.
 */

using FECFCoroutine = void;

struct FECFCoroutinePromise
{
	bool bHasFinished = false;
};

struct FECFCoroutineHandle 
{
	void resume() {}
	void destroy() {}

	FECFCoroutinePromise CoroPromise;
	FECFCoroutinePromise& promise() { return CoroPromise; }
};

#define co_await static_assert(false, "Trying to use co_await without coroutine support!")

#endif
