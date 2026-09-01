#pragma once

#include "CoreMinimal.h"
#include "PivotTools/X_PivotTypes.h"

namespace XPivotSnapshotSerialization
{
	/** 解析完整快照文档；失败时保持 OutSnapshots 原值不变。 */
	bool Deserialize(
		const FString& JsonString,
		TMap<FSoftObjectPath, FX_PivotSnapshot>& OutSnapshots,
		FString& OutError);
}
