// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistSettings_Advanced.h"

#include "BlueprintAssistCache.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "BlueprintAssistMisc/BACrashReporter.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"

UBASettings_Advanced::UBASettings_Advanced(const FObjectInitializer& ObjectInitializer)
{
	//~~~ Cache
	bSlowButAccurateSizeCaching = false;
	CacheSaveLocation = EBACacheSaveLocation::Project;
	bSaveBlueprintAssistCacheToFile = true;

	//~~~ Commands
	bRemoveLoopingCausedBySwapping = true;

	//~~~ Material Graph
	bGenerateUniqueGUIDForMaterialExpressions = false;

	//~~~ Cache
	bStoreCacheDataInPackageMetaData = false;
	bPrettyPrintCacheJSON = false;

	//~~~ Misc
	bUseCustomBlueprintActionMenu = false;
	bForceRefreshGraphAfterFormatting = false;
	bDisableBlueprintAssistPlugin = false;
	bHighlightBadComments = false;

	//~~~ Crash Reporter
	CrashReportingMethod = EBACrashReportingMethod::Ask;
	bDumpFormattingCrashNodes = true;
	bIncludeNodesInCrashReport = false;
	bIncludeSettingsInCrashReport = true;

	SaveSettingsDefaults();
}

void UBASettings_Advanced::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings_Advanced, CacheSaveLocation))
	{
		FBACache::Get().SaveCache();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings_Advanced, CrashReportingMethod))
	{
		if (CrashReportingMethod == EBACrashReportingMethod::Ask)
		{
			FBACrashReporter::Get().ShowNotification();
		}
	}

	UObject::PostEditChangeProperty(PropertyChangedEvent);
}

TSharedRef<IDetailCustomization> FBASettingsDetails_Advanced::MakeInstance()
{
	return MakeShared<FBASettingsDetails_Advanced>();
}

void FBASettingsDetails_Advanced::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& MiscCategory = DetailBuilder.EditCategory("Misc");
	auto& BACache = FBACache::Get();

	const FString CachePath = BACache.GetCachePath(true);

	const auto DeleteSizeCache = [&BACache]()
	{
		static FText Title = FText::FromString("Delete cache file");
		static FText Message = FText::FromString("Are you sure you want to delete the cache file?");

#if BA_UE_VERSION_OR_LATER(5, 3)
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, Title);
#else
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title);
#endif
		if (Result == EAppReturnType::Yes)
		{
			BACache.DeleteCache();
		}

		return FReply::Handled();
	};

	MiscCategory.AddCustomRow(FText::FromString("Delete cache file"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Delete cache file"))
			.Font(BA_GET_FONT_STYLE(TEXT("PropertyWindow.NormalFont")))
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(5).AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString("Delete cache file"))
				.ToolTipText(FText::FromString(FString::Printf(TEXT("Delete cache file located at: %s"), *CachePath)))
				.OnClicked_Lambda(DeleteSizeCache)
			]
		];
}
