// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFDelayTicksBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnECFDelayTicksBPEvent, bool, bStopped);

UCLASS()
class ENHANCEDCODEFLOW_API UECFDelayTicksBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnECFDelayTicksBPEvent OnComplete;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", ToolTip = "延迟指定帧数后执行动作", DisplayName = "ECF - 延迟帧执行-协程"), Category = "ECF")
	static UECFDelayTicksBP* ECFDelayTicks(const UObject* WorldContextObject, int32 DelayTicks, FECFActionSettings Settings, FECFHandleBP& Handle);
};
