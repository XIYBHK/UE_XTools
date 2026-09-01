#include "PivotTools/X_PivotSnapshotSerialization.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool XPivotSnapshotSerialization::Deserialize(
	const FString& JsonString,
	TMap<FSoftObjectPath, FX_PivotSnapshot>& OutSnapshots,
	FString& OutError)
{
	OutError.Reset();

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("解析 JSON 失败");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SnapshotsArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("Snapshots"), SnapshotsArray) || !SnapshotsArray)
	{
		OutError = TEXT("JSON 格式错误：缺少 Snapshots 数组");
		return false;
	}

	TMap<FSoftObjectPath, FX_PivotSnapshot> ParsedSnapshots;
	ParsedSnapshots.Reserve(SnapshotsArray->Num());
	for (int32 Index = 0; Index < SnapshotsArray->Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& Value = (*SnapshotsArray)[Index];
		const TSharedPtr<FJsonObject>* SnapshotObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(SnapshotObject) || !SnapshotObject || !SnapshotObject->IsValid())
		{
			OutError = FString::Printf(TEXT("Snapshots[%d] 不是有效对象"), Index);
			return false;
		}

		FString MeshPathString;
		double CenterX = 0.0;
		double CenterY = 0.0;
		double CenterZ = 0.0;
		if (!(*SnapshotObject)->TryGetStringField(TEXT("MeshPath"), MeshPathString)
			|| !(*SnapshotObject)->TryGetNumberField(TEXT("CenterX"), CenterX)
			|| !(*SnapshotObject)->TryGetNumberField(TEXT("CenterY"), CenterY)
			|| !(*SnapshotObject)->TryGetNumberField(TEXT("CenterZ"), CenterZ))
		{
			OutError = FString::Printf(TEXT("Snapshots[%d] 缺少必需字段"), Index);
			return false;
		}

		FX_PivotSnapshot Snapshot;
		Snapshot.MeshPath = FSoftObjectPath(MeshPathString);
		Snapshot.BoundsCenter = FVector(CenterX, CenterY, CenterZ);
		if (!Snapshot.IsValid() || Snapshot.BoundsCenter.ContainsNaN())
		{
			OutError = FString::Printf(TEXT("Snapshots[%d] 包含无效路径或坐标"), Index);
			return false;
		}

		FString TimestampString;
		if ((*SnapshotObject)->TryGetStringField(TEXT("Timestamp"), TimestampString)
			&& !FDateTime::Parse(TimestampString, Snapshot.Timestamp))
		{
			OutError = FString::Printf(TEXT("Snapshots[%d] 时间戳无效"), Index);
			return false;
		}

		ParsedSnapshots.Add(Snapshot.MeshPath, Snapshot);
	}

	OutSnapshots = MoveTemp(ParsedSnapshots);
	return true;
}
