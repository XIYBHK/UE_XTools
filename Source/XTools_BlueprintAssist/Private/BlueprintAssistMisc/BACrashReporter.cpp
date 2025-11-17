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


namespace BAXmlUtils
{
	static FString ParseCallStack(const FString& Callstack)
	{
		// The callstack is missing linebreaks, we need to re-insert line breaks and also clean the filepath
		// Each line should be in the format
		// Module!Function [File:Line]
		// UnrealEditor_BlueprintAssist!FEdGraphFormatter::FormatNode() [F:\Unreal\Project\Plugins\Developer\BlueprintAssist\Source\BlueprintAssist\Private\BlueprintAssistFormatters\EdGraphFormatter.cpp:50]
		const FRegexPattern Pattern(TEXT("(.*?)\\s*\\[(.*?)\\]"));
		FRegexMatcher Matcher(Pattern, Callstack);

		TStringBuilder<2048> FormattedCallstack;
		int32 LastMatchEnd = 0;

		while (Matcher.FindNext())
		{
			FString ModuleAndFunc = Matcher.GetCaptureGroup(1).TrimStartAndEnd();
			FString FileAndNum = Matcher.GetCaptureGroup(2);
			FString FilePath, LineNum;
			FileAndNum.Split(":", &FilePath, &LineNum, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			FPaths::NormalizeFilename(FilePath);
			// UE_LOG(LogTemp, Warning, TEXT("1: %s\n2: %s\n3: %s"), *ModuleAndFunc, *FilePath, *LineNum);

			// Remove everything in the filepath after /Source/
			const FString SearchTerm = TEXT("/Source/");
			int32 FoundIndex = FilePath.Find(SearchTerm, ESearchCase::IgnoreCase);
			if (FoundIndex != INDEX_NONE)
			{
				FilePath = FilePath.RightChop(FoundIndex + SearchTerm.Len());
			}

			// put it back in the original format
			FormattedCallstack.Appendf(TEXT("%s [%s:%s]"), *ModuleAndFunc, *FilePath, *LineNum);
			FormattedCallstack.Append(LINE_TERMINATOR);

			LastMatchEnd = Matcher.GetMatchEnding();
		}

		// handle the rest of the string which should be (kernel32 ntdll)
		if (LastMatchEnd < Callstack.Len())
		{
			FString Leftover = Callstack.RightChop(LastMatchEnd).TrimStartAndEnd();

			if (!Leftover.IsEmpty())
			{
				TArray<FString> Words;
				Leftover.ParseIntoArrayWS(Words);

				for (const FString& Word : Words)
				{
					FormattedCallstack.Append(Word);
					FormattedCallstack.Append(LINE_TERMINATOR);
				}
			}
		}

		return FormattedCallstack.ToString();
	}

	static bool FindBlueprintAssistVersion(FXmlFile& File, FString& OutVersion)
	{
		if (FXmlNode* FGenericCrashContext = File.GetRootNode())
		{
			if (FXmlNode* EnabledPlugins = FGenericCrashContext->FindChildNode("EnabledPlugins"))
			{
				TArray<FXmlNode*> PluginNodes = EnabledPlugins->GetChildrenNodes();
				for (FXmlNode* PluginNode : PluginNodes)
				{
					TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PluginNode->GetContent().Replace(TEXT("&quot;"), TEXT("\"")));

					TSharedPtr<FJsonObject> JsonObject;
					if (FJsonSerializer::Deserialize(Reader, JsonObject))
					{
						FString FriendlyName;
						if (JsonObject->TryGetStringField(TEXT("FriendlyName"), FriendlyName) && FriendlyName == "XTools")
						{
							if (JsonObject->TryGetStringField(TEXT("VersionName"), OutVersion))
							{
								return true;
							}
						}
					}
				}
			}
		}

		return false;
	}

	/** Modified version of FXmlFile::WriteNodeHierarchy which filters out all potentially user identifying info from the default crash report */
	static void WriteNodeHierarchyFiltered(const FXmlNode& Node, const FString& Indent, FString& Output, const FDateTime& CrashTime)
	{
		const static TSet<FString> ValidTags
		{
			// "EnabledPlugins",
			// "Plugin",
			"FGenericCrashContext",
			"RuntimeProperties",
			"CrashGUID",
			"IsEnsure",
			"IsStall",
			"IsAssert",
			"CrashType",
			"ErrorMessage",
			"CrashReporterMessage",
			"IsWithDebugInfo",
			"IsSourceDistribution",
			"BuildConfiguration",
			"PlatformName",
			"PlatformFullName",
			"PlatformNameIni",
			"EngineMode",
			"EngineVersion",
			"EngineCompatibleVersion",
			"BuildVersion",
			"IsUERelease",
			"IsRequestingExit",
			"SourceContext",
			"UserDescription",
			"UserActivityHint",
			"Misc.NumberOfCores",
			"Misc.NumberOfCoresIncludingHyperthreads",
			"Misc.Is64bitOperatingSystem",
			"Misc.CPUVendor",
			"Misc.CPUBrand",
			"Misc.OSVersionMajor",
			"Misc.OSVersionMinor",
			"Misc.PrimaryGPUBrand",
			"CallStack",
			"EngineData",
			"MatchingDPStatus",
			"RHI.IntegratedGPU",
			"RHI.DriverDenylisted",
			"RHI.D3DDebug",
			"RHI.DRED",
			"RHI.DREDMarkersOnly",
			"RHI.DREDContext",
			"RHI.Aftermath",
			"RHI.RHIName",
			"RHI.AdapterName",
			"RHI.UserDriverVersion",
			"RHI.InternalDriverVersion",
			"RHI.DriverDate",
			"RHI.FeatureLevel",
			"RHI.GPUVendor",
			"RHI.DeviceId",
			"DeviceProfile.Name",
			"Platform.AppHasFocus",
			"BAGraphHint",
			"BAAssetHint",
		};

		if (!ValidTags.Contains(Node.GetTag()))
		{
			return;
		}

		// Write the tag
		Output += Indent + FString::Printf(TEXT("<%s"), *Node.GetTag());
		for (const FXmlAttribute& Attribute : Node.GetAttributes())
		{
			FString EscapedValue = Attribute.GetValue();
			EscapedValue.ReplaceInline(TEXT("&"), TEXT("&amp;"), ESearchCase::CaseSensitive);
			EscapedValue.ReplaceInline(TEXT("\""), TEXT("&quot;"), ESearchCase::CaseSensitive);
			EscapedValue.ReplaceInline(TEXT("'"), TEXT("&apos;"), ESearchCase::CaseSensitive);
			EscapedValue.ReplaceInline(TEXT("<"), TEXT("&lt;"), ESearchCase::CaseSensitive);
			EscapedValue.ReplaceInline(TEXT(">"), TEXT("&gt;"), ESearchCase::CaseSensitive);
			Output += FString::Printf(TEXT(" %s=\"%s\""), *Attribute.GetTag(), *EscapedValue);
		}

		// Write the node contents
		const FXmlNode* FirstChildNode = Node.GetFirstChildNode();
		if (FirstChildNode == nullptr)
		{
			FString Content = Node.GetContent();

			// HACK: FXMLFile will read the content and remove all line breaks
			// Bugsplat needs line breaks to display the callstack, so handle this case 
			if (Node.GetTag() == "CallStack")
			{
				Content = ParseCallStack(Content);
			}

			if (Content.Len() == 0)
			{
				Output += TEXT("/>") LINE_TERMINATOR;
			}
			else
			{
				Output += TEXT(">") + Content + FString::Printf(TEXT("</%s>"), *Node.GetTag()) + LINE_TERMINATOR;
			}
		}
		else
		{
			Output += TEXT(">") LINE_TERMINATOR;

			// write the time when the crash actually occured (bugsplat only shows when you submit the report)
			if (Node.GetTag() == "RuntimeProperties")
			{
				Output += Indent + TEXT("\t") + FString::Printf(TEXT("<CrashTime>%s</CrashTime>"), *CrashTime.ToString()) + LINE_TERMINATOR;
			}

			for (const FXmlNode* ChildNode = FirstChildNode; ChildNode != nullptr; ChildNode = ChildNode->GetNextNode())
			{
				WriteNodeHierarchyFiltered(*ChildNode, Indent + TEXT("\t"), Output, CrashTime);
			}
			Output += Indent + FString::Printf(TEXT("</%s>"), *Node.GetTag()) + LINE_TERMINATOR;
		}
	}

	/** Copy of FXmlFile::Save but calls our the modified WriteNodeHierarchy function instead  */
	static bool GenerateFilteredXML(FXmlFile& File, const FString& Path, const FDateTime& CrashTime)
	{
		FString Xml = TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") LINE_TERMINATOR;

		const FXmlNode* CurrentNode = File.GetRootNode();
		if (CurrentNode != nullptr)
		{
			WriteNodeHierarchyFiltered(*CurrentNode, FString(), Xml, CrashTime);
		}

		if (!FFileHelper::SaveStringToFile(Xml, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return false;
		}

		return true;
	}
}

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
	// case EBACrashReportingMethod::Always:
	// 	PendingReports = GetUnsentReports();
	// 	SendReports();
	// 	break;
	case EBACrashReportingMethod::Never:
		break;
	default: ;
	}

	// FCoreDelegates::OnShutdownAfterError.AddRaw(this, &FBACrashReporter::WriteNodeGraph);
	// FCoreDelegates::OnHandleSystemError.AddRaw(this, &FBACrashReporter::WriteNodeGraph);
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

	if (BA_DEBUG("TestCrashReport"))
	{
		DataRouterUrl = FString::Printf(TEXT("https://blueprintassist-test.bugsplat.com/post/ue4/blueprintassist/%s"), *Report.Version);
	}

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

	// limit to 5 most recent crash reports and ignore the rest
	TArray<FString> IgnoredReports;

	IFileManager& FileManager = IFileManager::Get();

	double ThisTime = 0;
	{
		SCOPE_SECONDS_COUNTER(ThisTime);
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
										if (!BAXmlUtils::FindBlueprintAssistVersion(XmlFile, Report.Version))
										{
											// fallback to the current plugin version
											FPluginDescriptor PluginDesc = IPluginManager::Get().FindPlugin("XTools")->GetDescriptor();
											Report.Version = PluginDesc.VersionName;
										}

										UnsentReports.Add(Report);
									}
									else
									{
										IgnoredReports.Add(ReportId);
									}
								}
							}
						}
					}
				}
			}

			return true;
		});
	}

	UE_LOG(LogBlueprintAssist, Log, TEXT("Scanning crash logs took %.2fms"), ThisTime * 1000);
	WriteSentCrashesToLog(IgnoredReports);

	return UnsentReports;
}

void FBACrashReporter::HandleCrashUploadCompleted(const FString& ReportId, bool bSucceeded)
{
	if (bSucceeded)
	{
		SuccessfullyParsed.Add(ReportId);
	}

	// if we don't have any reports to send then we are done
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
	// no point adding to the file if we didn't add anything
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
			FXmlFile XmlFile;
			if (XmlFile.LoadFile(CrashContextPath))
			{
				const FString NewPath = FBAPaths::BACrashDir() / Report.ReportId / FBAPaths::BACrashContextName;
				const FDateTime CrashTime = IFileManager::Get().GetTimeStamp(*CrashContextPath);
				BAXmlUtils::GenerateFilteredXML(XmlFile, NewPath, CrashTime);
			}
		}
	}
}
