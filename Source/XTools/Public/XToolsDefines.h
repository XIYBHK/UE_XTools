// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Plugin version
#define XTOOLS_VERSION_MAJOR 1
#define XTOOLS_VERSION_MINOR 0
#define XTOOLS_VERSION_PATCH 0

// Debugging
#if !UE_BUILD_SHIPPING
#define XTOOLS_DEBUG 1
#else
#define XTOOLS_DEBUG 0
#endif

// Platform specific macros
#if PLATFORM_WINDOWS
#define XTOOLS_WINDOWS 1
#else
#define XTOOLS_WINDOWS 0
#endif

// Feature toggles
#define XTOOLS_FEATURE_PARENT_FINDER 1
#define XTOOLS_FEATURE_DEBUG_DRAWING 1

// Log category
DECLARE_LOG_CATEGORY_EXTERN(LogXTools, Log, All);

// Parent finder settings
static constexpr int32 XTOOLS_MAX_PARENT_DEPTH = 100;
