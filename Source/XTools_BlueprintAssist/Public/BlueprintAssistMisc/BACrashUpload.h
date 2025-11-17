// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

class SNotificationItem;
class UEdGraphNode;


DECLARE_DELEGATE_TwoParams(FOnCrashUploadComplete, const FString& /* CrashId */, bool /* bSuccess */);

/**
 * This class is a slightly modified version of the engine's CrashUpload class 
 */
struct XTOOLS_BLUEPRINTASSIST_API FBACrashUpload
{
	FHttpRequestPtr UploadRequest;

	bool SendCrashReport(const FString& CrashId, const FString& DataRouterUrl, const TArray<FString>& FilesToSend, FOnCrashUploadComplete OnCompleteDelegate);

	void CancelRequest();
};
