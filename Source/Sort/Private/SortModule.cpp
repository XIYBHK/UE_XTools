/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "SortModule.h"
#include "SortAPI.h"

#define LOCTEXT_NAMESPACE "FSortModule"

DEFINE_LOG_CATEGORY(LogSort);

void FSortModule::StartupModule()
{
    UE_LOG(LogSort, Log, TEXT("Sort module started"));
}

void FSortModule::ShutdownModule()
{
    UE_LOG(LogSort, Log, TEXT("Sort module shutdown"));
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FSortModule, Sort) 