// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistSettings_Advanced.h"

UBASettings_Advanced::UBASettings_Advanced(const FObjectInitializer& ObjectInitializer)
{
	//~~~ Commands
	bRemoveLoopingCausedBySwapping = true;

	//~~~ Material Graph
	bEnableMaterialGraphPinHoverFix = false; // Workaround
	bGenerateUniqueGUIDForMaterialExpressions = false;

	//~~~ Cache
	bStoreCacheDataInPackageMetaData = false;
	bPrettyPrintCacheJSON = false;

	//~~~ Misc
	bUseCustomBlueprintActionMenu = false;
	bForceRefreshGraphAfterFormatting = false;
}
