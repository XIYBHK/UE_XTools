// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistMisc/BACrashReporter.h"

#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistUtils.h"
#include "Editor.h"
#include "HttpModule.h"
#include "XmlFile.h"
#include "BlueprintAssistMisc/BACrashReportDialog.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/LazySingleton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Windows/WindowsPlatformCrashContext.h"
#include "Serialization/JsonSerializer.h"
#include "Stats/StatsMisc.h"

const TCHAR* const FBAPaths::BACrashContextName = TEXT("BlueprintAssistCrashContext.xml");
const TCHAR* const FBAPaths::BANodesName = TEXT("Nodes.txt");
const TCHAR* const FBAPaths::BAFormattingSettingsName = TEXT("BASettings_Formatting.ini");
const TCHAR* const FBAPaths::BAFeaturesSettingsName = TEXT("BASettings_Features.ini");

FString FBAPaths::CrashDir()
{
	return FPaths::ProjectSavedDir() / TEXT("Crashes");
}

FString FBAPaths::BACrashDir()
{
	return CrashDir() / TEXT("BACrashData");
}

const TCHAR* FBAPaths::CrashContextRuntimeXMLName()
{
	return FGenericCrashContext::CrashContextRuntimeXMLNameW;
}

FString FBAPaths::SentLogFile()
{
	return CrashDir() / TEXT("BASentCrashes.log");
}

FBACrashReporter& FBACrashReporter::Get()
{
	return TLazySingleton<FBACrashReporter>::Get();
}

void FBACrashReporter::TearDown()
{
	TLazySingleton<FBACrashReporter>::TearDown();
}

void FBACrashReporter::Init()
{
	switch (UBASettings_Advanced::Get().CrashReportingMethod)
	{
	case EBACrashReportingMethod::Ask:
		ShowNotification();
		break;
	case EBACrashReportingMethod::Never:
		break;
	default: ;
	}
}

void FBACrashReporter::ShowNotification()
{
	PendingReports = GetUnsentReports();
	if (PendingReports.Num() == 0)
	{
		return;
	}

	if (AskToSendNotification.IsValid())
	{
		return;
	}

	FNotificationInfo Info(INVTEXT("Detected Blueprint Assist related crashes. Open crash reporter?"));

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		INVTEXT("Yes"),
		INVTEXT("Opens the crash reporter dialog"),
		FSimpleDelegate::CreateRaw(this, &FBACrashReporter::HandleYes),
		SNotificationItem::CS_None
	));

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		INVTEXT("No"),
		INVTEXT("You will be asked again on next editor launch"),
		FSimpleDelegate::CreateRaw(this, &FBACrashReporter::HandleNo),
		SNotificationItem::CS_None
	));

	Info.ButtonDetails.Add(FNotificationButtonInfo(
		INVTEXT("Never"),
		INVTEXT("You won't be asked to send crash reports again"),
		FSimpleDelegate::CreateRaw(this, &FBACrashReporter::HandleNever),
		SNotificationItem::CS_None
	));

	Info.bFireAndForget = false;
	Info.bUseLargeFont = false;
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = false;

	AskToSendNotification = FSlateNotificationManager::Get().AddNotification(Info);
}

void FBACrashReporter::HandleYes()
{
	GenerateBACrashReports();
	CloseNotification();
	FSlateApplication::Get().AddWindow(SNew(SBACrashReportDialog));
}

void FBACrashReporter::HandleNo()
{
	CloseNotification();
}

void FBACrashReporter::HandleNever()
{
	CloseNotification();
	UBASettings_Advanced& BASettings = UBASettings_Advanced::GetMutable();
	BASettings.CrashReportingMethod = EBACrashReportingMethod::Never;
	BASettings.PostEditChange();
	BASettings.SaveConfig();
}

void FBACrashReporter::SendReport(const FBACrashReport& Report)
{
	FString DataRouterUrl = FString::Printf(TEXT("https://blueprintassist.bugsplat.com/post/ue4/blueprintassist/%s"), *Report.Version);

	TArray<FString> FilesToSend = { FBAPaths::BACrashDir() / Report.ReportId / FBAPaths::BACrashContextName };

	if (UBASettings_Advanced::Get().bIncludeSettingsInCrashReport)
	{
		const FString FormattingPath = FBAPaths::BACrashDir() / Report.ReportId / FBAPaths::BAFormattingSettingsName;
		UBASettings::GetMutable().SaveConfig(CPF_Config, *FormattingPath);
		FilesToSend.Add(FormattingPath);

		const FString FeaturesPath = FBAPaths::BACrashDir() / Report.ReportId / FBAPaths::BAFeaturesSettingsName;
		UBASettings_EditorFeatures::GetMutable().SaveConfig(CPF_Config, *FeaturesPath);
		FilesToSend.Add(FeaturesPath);
	}

	if (UBASettings_Advanced::Get().bIncludeNodesInCrashReport)
	{
		FilesToSend.Add(FBAPaths::BACrashDir() / Report.ReportId / FBAPaths::BANodesName);
	}

	FOnCrashUploadComplete OnCompleteDelegate = FOnCrashUploadComplete::CreateRaw(this, &FBACrashReporter::HandleCrashUploadCompleted);
	CrashUpload.SendCrashReport(Report.ReportId, DataRouterUrl, FilesToSend, OnCompleteDelegate);
}

void FBACrashReporter::CloseNotification()
{
	if (AskToSendNotification.IsValid())
	{
		AskToSendNotification.Pin()->SetExpireDuration(0.0f);
		AskToSendNotification.Pin()->SetFadeOutDuration(0.5f);
		AskToSendNotification.Pin()->ExpireAndFadeout();
	}
}

TArray<FString> FBACrashReporter::GetSentReportIds()
{
	const FString SentLogFile = FBAPaths::SentLogFile();

	TArray<FString> ParsedCrashes;
	if (FFileHelper::LoadFileToStringArray(ParsedCrashes, *SentLogFile))
	{
		return ParsedCrashes;
	}

	return {};
}

void FBACrashReporter::SendReports()
{
	if (PendingReports.Num() == 0)
	{
		return;
	}

	SendNextReport();

	FNotificationInfo Info = FNotificationInfo(INVTEXT("Sending Blueprint Assist crash reports"));
	Info.bUseThrobber = true;
	Info.bFireAndForget = false;
	Info.bUseSuccessFailIcons = true;
	Info.ExpireDuration = 3.0f;
	Info.ButtonDetails.Add(FNotificationButtonInfo(INVTEXT("Cancel"), FText(), FSimpleDelegate::CreateRaw(this, &FBACrashReporter::CancelSendingReports)));
	ProgressNotification = FSlateNotificationManager::Get().AddNotification(Info);
	ProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
}

TArray<FBACrashReport> FBACrashReporter::GetUnsentReports()
{
	TArray<FBACrashReport> UnsentReports;
	TArray<FString> SentCrashes = GetSentReportIds();

	IFileManager& FileManager = IFileManager::Get();

	FileManager.IterateDirectory(*FBAPaths::CrashDir(), [&](const TCHAR* DirName, bool bIsDirectory)
	{
		if (!bIsDirectory)
		{
			return true;
		}

		const FString ReportId = FPaths::GetCleanFilename(DirName);
		if (SentCrashes.Contains(ReportId))
		{
			return true;
		}

		const FString CrashContextPath = FString(DirName) / FBAPaths::CrashContextRuntimeXMLName();

		if (FileManager.FileExists(*CrashContextPath))
		{
			FXmlFile XmlFile;
			if (XmlFile.LoadFile(CrashContextPath))
			{
				if (FXmlNode* FGenericCrashContext = XmlFile.GetRootNode())
				{
					if (FXmlNode* RuntimeProperties = FGenericCrashContext->FindChildNode("RuntimeProperties"))
					{
						if (FXmlNode* CallStack = RuntimeProperties->FindChildNode("CallStack"))
						{
							if (CallStack->GetContent().Contains("BlueprintAssist"))
							{
								if (UnsentReports.Num() < 5)
								{
									FBACrashReport Report(ReportId);
									FPluginDescriptor PluginDesc = IPluginManager::Get().FindPlugin("BlueprintAssist")->GetDescriptor();
									Report.Version = PluginDesc.VersionName;
									UnsentReports.Add(Report);
								}
							}
						}
					}
				}
			}
		}

		return true;
	});

	return UnsentReports;
}

void FBACrashReporter::HandleCrashUploadCompleted(const FString& ReportId, bool bSucceeded)
{
	if (bSucceeded)
	{
		SuccessfullyParsed.Add(ReportId);
	}

	if (!SendNextReport())
	{
		if (ProgressNotification.IsValid())
		{
			if (SuccessfullyParsed.Num())
			{
				ProgressNotification.Pin()->SetText(INVTEXT("Sending crash reports complete"));
				ProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_Success);
			}
			else
			{
				ProgressNotification.Pin()->SetText(INVTEXT("Sending crash reports failed"));
				ProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_Fail);
			}

			ProgressNotification.Pin()->ExpireAndFadeout();
			ProgressNotification.Reset();
		}

		WriteSentCrashesToLog(SuccessfullyParsed);
		SuccessfullyParsed.Reset();
	}
}

bool FBACrashReporter::SendNextReport()
{
	if (PendingReports.Num() == 0)
	{
		return false;
	}

	SendReport(PendingReports.Pop());
	return true;
}

void FBACrashReporter::WriteSentCrashesToLog(const TArray<FString>& SentReports)
{
	if (SentReports.Num() == 0)
	{
		return;
	}

	TArray<FString> UpdatedSentCrashes = GetSentReportIds();
	UpdatedSentCrashes.Append(SentReports);

	const FString SentLogFilePath = FBAPaths::SentLogFile();
	FFileHelper::SaveStringArrayToFile(UpdatedSentCrashes, *SentLogFilePath);
}

void FBACrashReporter::CancelSendingReports()
{
	CrashUpload.CancelRequest();

	PendingReports.Reset();

	if (ProgressNotification.IsValid())
	{
		ProgressNotification.Pin()->SetText(INVTEXT("Sending crash report cancelled"));
		ProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_Fail);
		ProgressNotification.Pin()->ExpireAndFadeout();
		ProgressNotification.Reset();
	}

	WriteSentCrashesToLog(SuccessfullyParsed);
	SuccessfullyParsed.Reset();
}

void FBACrashReporter::GenerateBACrashReports()
{
	for (const FBACrashReport& Report : PendingReports)
	{
		FString CrashContextPath = FBAPaths::CrashDir() / Report.ReportId / FBAPaths::CrashContextRuntimeXMLName();

		IFileManager& FileManager = IFileManager::Get();
		if (FileManager.FileExists(*CrashContextPath))
		{
			// Basic implementation - copy the crash context file
			const FString NewPath = FBAPaths::BACrashDir() / Report.ReportId / FBAPaths::BACrashContextName;
			FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(NewPath));
			FPlatformFileManager::Get().GetPlatformFile().CopyFile(*NewPath, *CrashContextPath);
		}
	}
}
