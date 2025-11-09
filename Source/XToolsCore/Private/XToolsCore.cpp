/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "XToolsCore.h"

#define LOCTEXT_NAMESPACE "FXToolsCoreModule"

DEFINE_LOG_CATEGORY(LogXToolsCore);

void FXToolsCoreModule::StartupModule()
{
	UE_LOG(LogXToolsCore, Log, TEXT("XToolsCore module started"));
}

void FXToolsCoreModule::ShutdownModule()
{
	UE_LOG(LogXToolsCore, Log, TEXT("XToolsCore module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FXToolsCoreModule, XToolsCore)
