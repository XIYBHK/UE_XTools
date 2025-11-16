// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistMisc/BACrashUpload.h"

#include "BlueprintAssistUtils.h"
#include "HttpModule.h"
#include "JsonObjectConverter.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "PlatformHttp.h"
#include "BlueprintAssistMisc/BACrashReporter.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/EngineVersion.h"
#include "Serialization/JsonSerializer.h"

namespace BACrashUploadDefs
{
	const FString APIKey(TEXT("CrashReporter"));
	const FString AppEnvironment(TEXT("Dev"));
	const FString UploadType(TEXT("crashreports"));
}

struct FBACompressedHeader
{
	FString DirectoryName;
	FString FileName;
	int32 UncompressedSize;
	int32 FileCount;

	/** Serialization operator. */
	friend FArchive& operator <<(FArchive& Ar, FBACompressedHeader& Data)
	{
		// The 'CR1' marker prevents the data router backend to fallback to a backward compatibility version where
		// a buggy/incomplete header was written at the beginning of the stream and the correct/valid one at the end.
		uint8 Version[] = {'C', 'R', '1'};
		Ar.Serialize(Version, sizeof(Version));

		Data.DirectoryName.SerializeAsANSICharArray(Ar, 260);
		Data.FileName.SerializeAsANSICharArray(Ar, 260);
		Ar << Data.UncompressedSize;
		Ar << Data.FileCount;
		return Ar;
	}
};

struct FBACompressedCrashFile : FNoncopyable
{
	int32 CurrentFileIndex;
	FString Filename;
	TArray<uint8> Filedata;

	FBACompressedCrashFile(int32 InCurrentFileIndex, const FString& InFilename, const TArray<uint8>& InFiledata)
		: CurrentFileIndex(InCurrentFileIndex)
		, Filename(InFilename)
		, Filedata(InFiledata)
	{
	}

	/** Serialization operator. */
	friend FArchive& operator <<(FArchive& Ar, FBACompressedCrashFile& Data)
	{
		Ar << Data.CurrentFileIndex;
		Data.Filename.SerializeAsANSICharArray(Ar, 260);
		Ar << Data.Filedata;
		return Ar;
	}
};

struct FBACompressedData
{
	TArray<uint8> Data;
	int32 CompressedSize;
	int32 UncompressedSize;
	int32 FileCount;
};

static bool CompressData(const TArray<FString>& InPendingFiles, FBACompressedData& OutCompressedData, TArray<uint8>& OutPostData, FBACompressedHeader* OptionalHeader = nullptr)
{
	// Compress all files into one archive.
	constexpr int32 BufferSize = 32 * 1024 * 1024;

	TArray<uint8> UncompressedData;
	UncompressedData.Reserve(BufferSize);
	FMemoryWriter MemoryWriter(UncompressedData, false, true);

	if (OptionalHeader != nullptr)
	{
		// Write dummy to fill correct size
		MemoryWriter << *OptionalHeader;
	}

	int32 CurrentFileIndex = 0;

	// Loop to keep trying files until a send succeeds or we run out of files
	for (const FString& PathOfFileToUpload : InPendingFiles)
	{
		FString Filename = FPaths::GetCleanFilename(PathOfFileToUpload);

		if (!FFileHelper::LoadFileToArray(OutPostData, *PathOfFileToUpload))
		{
			UE_LOG(LogBlueprintAssist, Warning, TEXT("Failed to load crash report file %s"), *PathOfFileToUpload);
			continue;
		}

		// when we are sending the BA filtered crash context, use the default Unreal name so BugSplat recognizes it  
		if (Filename == FBAPaths::BACrashContextName)
		{
			Filename = FBAPaths::CrashContextRuntimeXMLName();
		}

		// UE_LOG(LogBlueprintAssist, Log, TEXT("CompressAndSendData compressing %d bytes ('%s')"), OutPostData.Num(), *PathOfFileToUpload);
		FBACompressedCrashFile FileToCompress(CurrentFileIndex, Filename, OutPostData);
		CurrentFileIndex++;

		MemoryWriter << FileToCompress;
	}

	if (OptionalHeader != nullptr)
	{
		FMemoryWriter MemoryHeaderWriter(UncompressedData);

		OptionalHeader->UncompressedSize = UncompressedData.Num();
		OptionalHeader->FileCount = CurrentFileIndex;

		MemoryHeaderWriter << *OptionalHeader;
	}

	int UncompressedSize = UncompressedData.Num();

	uint8* CompressedDataRaw = new uint8[UncompressedSize];

	OutCompressedData.FileCount = CurrentFileIndex;
	OutCompressedData.CompressedSize = UncompressedSize;
	OutCompressedData.UncompressedSize = UncompressedSize;
	const bool bResult = FCompression::CompressMemory(NAME_Zlib, CompressedDataRaw, OutCompressedData.CompressedSize, UncompressedData.GetData(), OutCompressedData.UncompressedSize);
	if (bResult)
	{
		// Copy compressed data into the array.
		OutCompressedData.Data.Append(CompressedDataRaw, OutCompressedData.CompressedSize);
		delete[] CompressedDataRaw;
		CompressedDataRaw = nullptr;
	}

	return bResult;
}

bool FBACrashUpload::SendCrashReport(const FString& CrashId, const FString& DataRouterUrl, const TArray<FString>& FilesToSend, FOnCrashUploadComplete OnCompleteDelegate)
{
	FBACompressedHeader CompressedHeader;
	CompressedHeader.DirectoryName = CrashId;
	CompressedHeader.FileName = CompressedHeader.DirectoryName + TEXT(".uecrash");

	TArray<FString> PendingFiles;
	for (const FString& File : FilesToSend)
	{
		// const FString FullPath = CrashPath / File;
		const FString FullPath = File;
		if (IFileManager::Get().FileExists(*FullPath))
		{
			UE_LOG(LogBlueprintAssist, Verbose, TEXT("Sending crash file: %s"), *FullPath);
			PendingFiles.Add(FullPath);
		}
	}

	if (PendingFiles.Num() == 0)
	{
		return false;
	}

	UE_LOG(LogBlueprintAssist, Log, TEXT("Sending crash report: %s"), *CrashId);

	/** Buffer to keep reusing for file content and other messages */
	TArray<uint8> PostData;
	FBACompressedData CompressedData;
	if (!CompressData(PendingFiles, CompressedData, PostData, &CompressedHeader))
	{
		UE_LOG(LogBlueprintAssist, Warning, TEXT("Failed to compress crash report files. Not sending."));
		return false;
	}

	PendingFiles.Empty();

	const FString UserId = FString::Printf(TEXT("%s|%s|%s"), *FPlatformMisc::GetLoginId(), *FPlatformMisc::GetEpicAccountId(), *FPlatformMisc::GetOperatingSystemId());

	const FString UrlParams = FString::Printf(
		TEXT("?AppID=%s&AppVersion=%s&AppEnvironment=%s&UploadType=%s&UserID=%s"),
		*FGenericPlatformHttp::UrlEncode(BACrashUploadDefs::APIKey),
		*FGenericPlatformHttp::UrlEncode(FEngineVersion::Current().ToString()),
		*FGenericPlatformHttp::UrlEncode(BACrashUploadDefs::AppEnvironment),
		*FGenericPlatformHttp::UrlEncode(BACrashUploadDefs::UploadType),
		*FGenericPlatformHttp::UrlEncode(UserId)
	);

	// with the way this class is used from BACrashReporter, this shouldn't ever happen
	if (UploadRequest.IsValid() && UploadRequest->GetStatus() == EHttpRequestStatus::Processing)
	{
		UploadRequest->CancelRequest();
		UploadRequest.Reset();
	}

	// Set up request for upload
	UploadRequest = FHttpModule::Get().CreateRequest();
	UploadRequest->SetVerb(TEXT("POST"));
	UploadRequest->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	UploadRequest->SetURL(DataRouterUrl + UrlParams);
	UploadRequest->SetContent(CompressedData.Data);

	UploadRequest->OnProcessRequestComplete().BindLambda([CrashId, OnCompleteDelegate, this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
	{
		if (Response)
		{
			const FString ResponseStr = Response->GetContentAsString();
			const FString ResponseDescription = EHttpResponseCodes::GetDescription(StaticCast<EHttpResponseCodes::Type>(Response->GetResponseCode())).ToString();
			if (bConnectedSuccessfully)
			{
				if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
				{
					TSharedPtr<FJsonValue> Data;
					if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseStr), Data))
					{
						const TSharedPtr<FJsonObject>* JsonObj = nullptr;
						if (Data->TryGetObject(JsonObj))
						{
							if (TSharedPtr<FJsonValue> SuccessField = JsonObj->Get()->TryGetField(TEXT("status")))
							{
								FString SuccessVal;
								if (SuccessField->TryGetString(SuccessVal) && SuccessVal == "success")
								{
									UE_LOG(LogBlueprintAssist, Log, TEXT("Successfully sent crash report %s %s"), *CrashId, *Response->GetContentAsString());
									OnCompleteDelegate.ExecuteIfBound(CrashId, true);
									UploadRequest.Reset();
									return;
								}
							}
						}
					}
				}
			}

			UE_LOG(LogBlueprintAssist, Warning, TEXT("Failed to send crash report %s Response=%s Content=%s"), *CrashId, *ResponseDescription, *Response->GetContentAsString());
		}
		else if (!bConnectedSuccessfully)
		{
			if (Request)
			{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
				UE_LOG(LogBlueprintAssist, Warning, TEXT("Failed to send crash report %s Reason=%s"), *CrashId, LexToString(Request->GetFailureReason()));
#else
				UE_LOG(LogBlueprintAssist, Warning, TEXT("Failed to send crash report %s Reason=Connection Failed"), *CrashId);
#endif
			}
			else
			{
				UE_LOG(LogBlueprintAssist, Warning, TEXT("Failed to send crash report %s Reason=Unknown"), *CrashId);
			}
		}

		OnCompleteDelegate.ExecuteIfBound(CrashId, false);
		UploadRequest.Reset();
	});

	UE_LOG(LogBlueprintAssist, Verbose, TEXT("Sending HTTP request: %s, Payload size: %d"), *UploadRequest->GetURL(), CompressedData.Data.Num());
	UploadRequest->ProcessRequest();
	return true;
}

void FBACrashUpload::CancelRequest()
{
	if (UploadRequest.IsValid() && UploadRequest->GetStatus() == EHttpRequestStatus::Processing)
	{
		UploadRequest->CancelRequest();
		UploadRequest.Reset();
	}
}
