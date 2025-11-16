// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BACrashUpload.h"

class SNotificationItem;
class UEdGraphNode;

struct FBAPaths
{
	static FString CrashDir();
	static FString BACrashDir();
	static const TCHAR* CrashContextRuntimeXMLName();
	static FString SentLogFile();

	static const TCHAR* const BACrashContextName;
	static const TCHAR* const BANodesName;
	static const TCHAR* const BAFormattingSettingsName;
	static const TCHAR* const BAFeaturesSettingsName;
};

struct FBACrashReport
{
	explicit FBACrashReport(const FString& InReportId) : ReportId(InReportId) {}

	FString ReportId;
	FString Version;
};

/**
 * On plugin launch, check the Saved/Crash folder and ask to send any BlueprintAssist related logs
 */
class XTOOLS_BLUEPRINTASSIST_API FBACrashReporter
{
public:

	static FBACrashReporter& Get();
	static void TearDown();
	void Init();

	void ShowNotification();
	void HandleYes();
	void HandleNo();
	void HandleNever();

	void SendReport(const FBACrashReport& Report);
	void CloseNotification();
	TArray<FString> GetSentReportIds();

	void SendReports();
	TArray<FBACrashReport> GetUnsentReports();

	void HandleCrashUploadCompleted(const FString& CrashId, bool bSucceeded);
	bool SendNextReport();

	void WriteSentCrashesToLog(const TArray<FString>& SentReports);

	void CancelSendingReports();

	void GenerateBACrashReports();

	const TArray<FBACrashReport>& GetPendingReports() const { return PendingReports; }

private:
	TWeakPtr<SNotificationItem> AskToSendNotification;
	TWeakPtr<SNotificationItem> ProgressNotification;

	TArray<FBACrashReport> PendingReports;
	TArray<FString> SuccessfullyParsed;
	
	FBACrashUpload CrashUpload;
};
