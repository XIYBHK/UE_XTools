/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "ObjectPoolEditor.h"

#define LOCTEXT_NAMESPACE "FObjectPoolEditorModule"

DEFINE_LOG_CATEGORY(LogObjectPoolEditor);

void FObjectPoolEditorModule::StartupModule()
{
	UE_LOG(LogObjectPoolEditor, Log, TEXT("ObjectPoolEditor module started"));
}

void FObjectPoolEditorModule::ShutdownModule()
{
	UE_LOG(LogObjectPoolEditor, Log, TEXT("ObjectPoolEditor module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FObjectPoolEditorModule, ObjectPoolEditor)

