// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistMisc/BAMiscUtils.h"

#include "PersonaModule.h"
#include "PropertyEditorClipboard.h"
#include "Editor/Kismet/Public/SSCSEditor.h"
#include "Logging/MessageLog.h"
#include "Misc/AsciiSet.h"
#include "Misc/Base64.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#if BA_UE_VERSION_OR_LATER(5, 0)
#include "SSubobjectEditor.h"
#endif

AActor* FBAMiscUtils::GetSCSNodeDefaultActor(TSharedPtr<BA_SUBOBJECT_EDITOR_TREE_NODE> Node, UBlueprint* Blueprint)
{
#if BA_UE_VERSION_OR_LATER(5, 0)
	FSubobjectData* Data = Node->GetDataSource();
	return Data ? const_cast<AActor*>(Data->GetObjectForBlueprint<AActor>(Blueprint)) : nullptr;
#elif BA_UE_VERSION_OR_LATER(4, 26)
	return Node->GetEditableObjectForBlueprint<AActor>(Blueprint);
#else
	return (Blueprint != nullptr && Blueprint->GeneratedClass != nullptr) ? Blueprint->GeneratedClass->GetDefaultObject<AActor>() : nullptr;
#endif
}

bool FBAMiscUtils::IsSCSActorNode(TSharedPtr<BA_SUBOBJECT_EDITOR_TREE_NODE> Node)
{
#if BA_UE_VERSION_OR_LATER(5, 0)
	FSubobjectData* Source = Node->GetDataSource();
	return Source ? Source->IsActor() : false;
#elif BA_UE_VERSION_OR_LATER(4, 26)
	return Node->IsActorNode();
#else
	return Node->GetNodeType() == FSCSEditorTreeNode::ENodeType::RootActorNode;
#endif
}

TArray<FString> FBAMiscUtils::ParseStringIntoArray(const FString& String, bool bToLower)
{
	constexpr FAsciiSet Delimiters("_,.|/;");

	int StartSlice = -1;

	TArray<FString> OutArray;

	// slice is a start index and end index
	TArray<TPair<int, int>> Slices;

	// UE_LOG(LogTemp, Warning, TEXT("%s"), *String);

	bool bIsPrevUpper = false;
	for (int i = 0; i < String.Len(); ++i)
	{
		const TCHAR& Char = String[i];
		const bool bSkipChar = FChar::IsWhitespace(Char) || Delimiters.Contains(Char);
		const bool bIsLastChar = i == String.Len() - 1;
		const bool bIsUpper = FChar::IsUpper(Char);

		if (StartSlice >= 0)
		{
			if (bSkipChar || bIsLastChar || (bIsUpper && !bIsPrevUpper))
			{
				const int EndSlice = bIsLastChar ? i : i - 1;
				// Slices.Add(TPair<int, int>(StartSlice, EndSlice));
				const int Length = EndSlice - StartSlice + 1;
				FString Slice = String.Mid(StartSlice, Length);
				if (bToLower)
				{
					Slice.ToLowerInline();
				}
				OutArray.Add(Slice);

				// reset the slice index or start it here if we don't skip the char
				StartSlice = bSkipChar ? -1 : i;
			}
		}
		else if (!bSkipChar && StartSlice < 0)
		{
			// if we are a new slice but we are the last char then we need a 1 length slice
			if (bIsLastChar)
			{
				TCHAR LowerChar = bToLower ? FChar::ToLower(Char) : Char;
				OutArray.Add(FString(&LowerChar));
			}
			else
			{
				StartSlice = i;
			}
		}

		bIsPrevUpper = bIsUpper;
	}

	// for (const FString& S : OutArray)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("%s"), *S);
	// }

	return OutArray;
}

FString FBAMiscUtils::ParseSearchTextByFilter(const FString& SearchText, const FString& FilterString)
{
	TArray<FString> SearchWords = ParseStringIntoArray(SearchText);

	int LastMatchingLetter = -1;
	for (int i = 0; i < SearchWords.Num(); ++i)
	{
		// add first letter of search word
		const FString& Word = SearchWords[i];

		// if the character matches and we aren't the last word use the character instead
		if (FilterString.Len() > i && FChar::ToLower(FilterString[i]) == FChar::ToLower(Word[0]) && i != SearchWords.Num())
		{
			LastMatchingLetter = i;
		}
		else
		{
			break;
		}
	}

	// make new search text by ignoring words when the first letter matches 
	FString NewSearchText;
	for (int i = 0; i < SearchWords.Num(); ++i)
	{
		// add first letter of search word
		const FString& Word = SearchWords[i].ToLower();
		if (i <= LastMatchingLetter)
		{
			NewSearchText += Word[0];
		}
		else
		{
			NewSearchText += Word;
		}
	}

	// UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *SearchText, *NewSearchText);

	return NewSearchText;
}

TSharedPtr<IAssetFamily> FBAMiscUtils::GetAssetFamilyForObject(UObject* Object)
{
	if (Object)
	{
		if (FPersonaModule* PersonaModule = FModuleManager::LoadModulePtr<FPersonaModule>("Persona"))
		{
			TSharedRef<IAssetFamily> AssetFamily = PersonaModule->CreatePersonaAssetFamily(Object);
			return AssetFamily;
		}
	}

	return nullptr;
}

void FBAMiscUtils::MessageLogError(const FText& ErrorMsg)
{
	FMessageLog MessageLog("BlueprintAssist");
#if BA_UE_VERSION_OR_LATER(5, 1)
	MessageLog.SetCurrentPage(INVTEXT("EdGraphFormatter"));
#else
	MessageLog.NewPage(INVTEXT("EdGraphFormatter"));
#endif

	MessageLog.Error(ErrorMsg);
	MessageLog.Notify(ErrorMsg, EMessageSeverity::Error, true);
}

bool FBAMiscUtils::IsBlueprintImplementableEvent(UFunction* Function)
{
	return Function && Function->HasAnyFunctionFlags(FUNC_Event) && !Function->HasAnyFunctionFlags(FUNC_Native);
}

bool FBAMiscUtils::IsBlueprintNativeEvent(UFunction* Function)
{
	return Function && Function->HasAnyFunctionFlags(FUNC_Event) && Function->HasAnyFunctionFlags(FUNC_Native);
}

FString FBAMiscUtils::GetInputChordName(const FInputChord& Chord)
{
	if (Chord.IsValidChord())
	{
		return Chord.GetInputText().ToString();
	}

	return "Unbound";
}

FString FBAMiscUtils::CompressString(const FString& InString, FName FormatName)
{
	FString Result;
	if (InString.IsEmpty())
	{
		return Result;
	}

	// Convert FString to a TArray<uint8> of UTF-8 data
	TArray<uint8> UncompressedData;
	const FTCHARToUTF8 Converter(*InString);
	UncompressedData.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());

	if (UncompressedData.Num() == 0)
	{
		return Result;
	}

	const int32 UncompressedSize = UncompressedData.Num();

	int64 MaxCompressedSize = -1;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	// UE5.5+ signature: GetMaximumCompressedSize(FName, int64&, int32)
	if (!FCompression::GetMaximumCompressedSize(FormatName, MaxCompressedSize, UncompressedSize))
#else
	// UE5.3-5.4 signature: GetMaximumCompressedSize(FName, int32, ECompressionFlags, int32)
	int32 MaxCompressedSize32 = 0;
	if (!FCompression::GetMaximumCompressedSize(FormatName, UncompressedSize, ECompressionFlags::COMPRESS_None, MaxCompressedSize32))
	{
		return Result;
	}
	MaxCompressedSize = MaxCompressedSize32;
	if (false) // Skip the common error handling below
#endif
	{
		return Result;
	}

	// Compress the data
	TArray<uint8> CompressedData;
	CompressedData.SetNumUninitialized(MaxCompressedSize);

	int32 CompressedSize = CompressedData.Num();
	if (!FCompression::CompressMemory(FormatName, CompressedData.GetData(), CompressedSize, UncompressedData.GetData(), UncompressedSize))
	{
		return Result;
	}

	// Trim the array to the actual compressed size
	CompressedData.SetNum(CompressedSize);

	TArray<uint8> FinalPayload;

	// Reserve header + data
	FinalPayload.Reserve(sizeof(int32) + CompressedSize);

	// Add the size header then data
	FinalPayload.Append(reinterpret_cast<const uint8*>(&UncompressedSize), sizeof(int32));
	FinalPayload.Append(CompressedData);

	Result = FBase64::Encode(FinalPayload);
	return Result;
}


bool FBAMiscUtils::DecompressString(const FString& InCompressedString, FString& OutDecompressedString, FName FormatName)
{
	if (InCompressedString.IsEmpty())
	{
		OutDecompressedString = TEXT("");
		return true;
	}

	TArray<uint8> CompressedPayload;
	if (!FBase64::Decode(InCompressedString, CompressedPayload))
	{
		return false;
	}

	if (CompressedPayload.Num() < sizeof(int32))
	{
		return false;
	}

	// Extract the size from the header
	int32 UncompressedSize;
	FMemory::Memcpy(&UncompressedSize, CompressedPayload.GetData(), sizeof(int32));

	// Prepare pointers to the data (after the size header)
	const uint8* CompressedDataPtr = CompressedPayload.GetData() + sizeof(int32);
	const int32 CompressedSize = CompressedPayload.Num() - sizeof(int32);

	// Decompress the data
	TArray<uint8> UncompressedData;
	UncompressedData.SetNumUninitialized(UncompressedSize);

	if (!FCompression::UncompressMemory(FormatName, UncompressedData.GetData(), UncompressedSize, CompressedDataPtr, CompressedSize))
	{
		return false;
	}

	// Convert the uncompressed TArray<uint8> (UTF-8) back to an FString
	FUTF8ToTCHAR Converter(reinterpret_cast<const char*>(UncompressedData.GetData()), UncompressedSize);
	OutDecompressedString = FString(Converter.Length(), Converter.Get());
	return true;
}

void FBAMiscUtils::WriteTextToFile(TCHAR* FullPath, TCHAR* Content)
{
	// from FCrashContextExtendedWriterImpl::OutputBuffer
	if (FullPath == nullptr || Content == nullptr)
	{
		return;
	}

	TUniquePtr<IFileHandle> File(IPlatformFile::GetPlatformPhysical().OpenWrite(FullPath));
	if (File)
	{
		FTCHARToUTF8 Converter(Content);
		File->Write((uint8*)Converter.Get(), Converter.Length());
		File->Flush();
	}
}

void FBAMiscUtils::ClipboardCopy(const FString& String)
{
#if BA_UE_VERSION_OR_LATER(5, 3)
	FPropertyEditorClipboard::ClipboardCopy(*String);
#else
	FPlatformApplicationMisc::ClipboardCopy(*String);
#endif
}

TSharedPtr<SNotificationItem> FBAMiscUtils::ShowSimpleSlateNotification(const FText& Msg, SNotificationItem::ECompletionState State = SNotificationItem::CS_Success, float ExpireDuration)
{
	FNotificationInfo Info(Msg);
	Info.bUseSuccessFailIcons = true;
	Info.ExpireDuration = ExpireDuration;
	TSharedPtr<SNotificationItem> Notif = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notif)
	{
		Notif->SetCompletionState(State);
	}

	return Notif;
}
