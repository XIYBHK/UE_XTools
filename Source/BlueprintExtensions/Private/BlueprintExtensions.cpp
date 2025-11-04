/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "BlueprintExtensions.h"

#define LOCTEXT_NAMESPACE "FBlueprintExtensionsModule"

DEFINE_LOG_CATEGORY(LogBlueprintExtensions);

void FBlueprintExtensionsModule::StartupModule()
{
	UE_LOG(LogBlueprintExtensions, Log, TEXT("BlueprintExtensions module started"));
}

void FBlueprintExtensionsModule::ShutdownModule()
{
	UE_LOG(LogBlueprintExtensions, Log, TEXT("BlueprintExtensions module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintExtensionsModule, BlueprintExtensions)

