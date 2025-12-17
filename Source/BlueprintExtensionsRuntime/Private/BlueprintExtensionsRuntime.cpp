/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "BlueprintExtensionsRuntime.h"

#define LOCTEXT_NAMESPACE "FBlueprintExtensionsRuntimeModule"

DEFINE_LOG_CATEGORY(LogBlueprintExtensionsRuntime);

void FBlueprintExtensionsRuntimeModule::StartupModule()
{
	UE_LOG(LogBlueprintExtensionsRuntime, Log, TEXT("BlueprintExtensionsRuntime module started"));
}

void FBlueprintExtensionsRuntimeModule::ShutdownModule()
{
	UE_LOG(LogBlueprintExtensionsRuntime, Log, TEXT("BlueprintExtensionsRuntime module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintExtensionsRuntimeModule, BlueprintExtensionsRuntime);

