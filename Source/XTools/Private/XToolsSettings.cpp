// Copyright Epic Games, Inc. All Rights Reserved.

#include "XToolsSettings.h"

#define LOCTEXT_NAMESPACE "XToolsSettings"

UXToolsSettings::UXToolsSettings()
{
    // 设置分类
    CategoryName = TEXT("Plugins");
    SectionName = TEXT("XTools");
}

FName UXToolsSettings::GetCategoryName() const
{
    return FName(TEXT("Plugins"));
}

#if WITH_EDITOR
FText UXToolsSettings::GetSectionText() const
{
    return LOCTEXT("XToolsSettingsSection", "XTools");
}
#endif

const UXToolsSettings* UXToolsSettings::Get()
{
    return GetDefault<UXToolsSettings>();
}

UXToolsSettings* UXToolsSettings::GetMutable()
{
    return GetMutableDefault<UXToolsSettings>();
}

#undef LOCTEXT_NAMESPACE

