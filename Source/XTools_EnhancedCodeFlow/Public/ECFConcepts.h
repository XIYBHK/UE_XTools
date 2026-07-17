// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

#pragma once

#include "UObject/SoftObjectPtr.h"

#include <type_traits>

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L && __has_include(<concepts>)

#define ECF_WITH_CONCEPTS 1
#include <concepts>

template<typename T> struct TIsECFSoftPointer : std::false_type {};
template<typename U> struct TIsECFSoftPointer<TSoftObjectPtr<U>> : std::true_type {};
template<typename U> struct TIsECFSoftPointer<TSoftClassPtr<U>> : std::true_type {};

template<typename T>
concept CIsSoftPtrType = TIsECFSoftPointer<T>::value;

#else

#define ECF_WITH_CONCEPTS 0
#define CIsSoftPtrType typename

#endif
