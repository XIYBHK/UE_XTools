// Copyright Epic Games, Inc. All Rights Reserved.

#include "FieldSystemExtensions.h"

#define LOCTEXT_NAMESPACE "FFieldSystemExtensionsModule"

DEFINE_LOG_CATEGORY(LogFieldSystemExtensions);

void FFieldSystemExtensionsModule::StartupModule()
{
	UE_LOG(LogFieldSystemExtensions, Log, TEXT("FieldSystemExtensions module started"));
}

void FFieldSystemExtensionsModule::ShutdownModule()
{
	UE_LOG(LogFieldSystemExtensions, Log, TEXT("FieldSystemExtensions module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFieldSystemExtensionsModule, FieldSystemExtensions)

