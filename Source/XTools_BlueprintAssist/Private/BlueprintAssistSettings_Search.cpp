// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistSettings_Search.h"

UBlueprintAssistSettings_Search& UBlueprintAssistSettings_Search::Get()
{
	return *GetMutableDefault<UBlueprintAssistSettings_Search>();
}
