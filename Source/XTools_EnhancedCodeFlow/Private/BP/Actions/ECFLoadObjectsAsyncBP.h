// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

#pragma once

#include "../ECFActionBP.h"
#include "ECFLoadObjectsAsyncBP.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnECFLoadObjectsAsyncBPEvent, bool, bStopped);

UCLASS()
class XTOOLS_ENHANCEDCODEFLOW_API UECFLoadObjectsAsyncBP : public UECFActionBP
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable, Category = "XTools|ECF|异步加载", meta = (DisplayName = "完成"))
	FOnECFLoadObjectsAsyncBPEvent OnComplete;

	UFUNCTION(BlueprintCallable, Category = "XTools|ECF|异步加载", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "Settings", DisplayName = "ECF - 异步加载对象", Keywords = "异步 加载 软引用 资源", ToolTip = "异步加载软对象路径列表，并在全部加载完成或动作被停止时触发完成回调。"))
	static UECFLoadObjectsAsyncBP* ECFLoadObjectsAsync(
		UPARAM(DisplayName = "世界上下文") const UObject* WorldContextObject,
		UPARAM(DisplayName = "待加载对象") const TArray<FSoftObjectPath>& ObjectsToLoad,
		UPARAM(DisplayName = "动作设置") FECFActionSettings Settings,
		UPARAM(DisplayName = "动作句柄") FECFHandleBP& Handle);
};
