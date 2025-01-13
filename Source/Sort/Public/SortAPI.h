#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#define IS_SORT_API PLATFORM_WINDOWS
#else
#define IS_SORT_API 0
#endif

#if IS_SORT_API
#define SORT_API DLLEXPORT
#else
#define SORT_API DLLIMPORT
#endif 