// Copyright Epic Games, Inc. All Rights Reserved.

#include "XFieldSystemLibrary.h"
#include "XFieldSystemActor.h"
#include "FieldSystemExtensions.h"

UFieldSystemMetaDataFilter* UXFieldSystemLibrary::CreateBasicFilter(
	TEnumAsByte<EFieldObjectType> ObjectType,
	TEnumAsByte<EFieldFilterType> FilterType,
	TEnumAsByte<EFieldPositionType> PositionType)
{
	UFieldSystemMetaDataFilter* Filter = NewObject<UFieldSystemMetaDataFilter>();
	
	if (Filter)
	{
		Filter->SetMetaDataFilterType(FilterType, ObjectType, PositionType);
	}

	return Filter;
}

UFieldSystemMetaDataFilter* UXFieldSystemLibrary::CreateExcludeCharacterFilter()
{
	// 只影响Destruction对象，排除角色
	return CreateBasicFilter(
		EFieldObjectType::Field_Object_Destruction,
		EFieldFilterType::Field_Filter_Dynamic,
		EFieldPositionType::Field_Position_CenterOfMass
	);
}

UFieldSystemMetaDataFilter* UXFieldSystemLibrary::CreateDestructionOnlyFilter()
{
	return CreateBasicFilter(
		EFieldObjectType::Field_Object_Destruction,
		EFieldFilterType::Field_Filter_All,
		EFieldPositionType::Field_Position_CenterOfMass
	);
}

UFieldSystemMetaDataFilter* UXFieldSystemLibrary::GetOrCreateActorFilter(AXFieldSystemActor* Actor)
{
	if (!Actor)
	{
		UE_LOG(LogFieldSystemExtensions, Warning, TEXT("XFieldSystemLibrary: Invalid Actor provided to GetOrCreateActorFilter"));
		return nullptr;
	}

	// 如果Actor已有缓存的筛选器，返回它
	UFieldSystemMetaDataFilter* CachedFilter = Actor->GetCachedFilter();
	if (CachedFilter)
	{
		UE_LOG(LogFieldSystemExtensions, Verbose, TEXT("XFieldSystemLibrary: Using cached filter from %s"), *Actor->GetName());
		return CachedFilter;
	}

	// 否则创建新的筛选器并缓存
	UFieldSystemMetaDataFilter* NewFilter = Actor->CreateMetaDataFilter();
	if (NewFilter)
	{
		Actor->SetCachedFilter(NewFilter);
		UE_LOG(LogFieldSystemExtensions, Log, TEXT("XFieldSystemLibrary: Created and cached new filter for %s"), *Actor->GetName());
	}
	
	return NewFilter;
}

