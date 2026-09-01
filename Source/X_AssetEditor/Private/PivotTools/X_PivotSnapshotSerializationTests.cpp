/*
 * Pivot 快照序列化自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "PivotTools/X_PivotSnapshotSerialization.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXPivotSnapshotDeserializeIsAtomic,
	"XTools.AssetEditor.Pivot.SnapshotDeserializeIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXPivotSnapshotDeserializeIsAtomic::RunTest(const FString& Parameters)
{
	const FSoftObjectPath ExistingPath(TEXT("/Game/Test/SM_Existing.SM_Existing"));
	FX_PivotSnapshot ExistingSnapshot;
	ExistingSnapshot.MeshPath = ExistingPath;
	ExistingSnapshot.BoundsCenter = FVector(1.0, 2.0, 3.0);

	TMap<FSoftObjectPath, FX_PivotSnapshot> Snapshots;
	Snapshots.Add(ExistingPath, ExistingSnapshot);
	FString Error;

	TestFalse(TEXT("损坏JSON应拒绝加载"),
		XPivotSnapshotSerialization::Deserialize(TEXT("{"), Snapshots, Error));
	TestTrue(TEXT("损坏JSON后应保留现有快照"), Snapshots.Contains(ExistingPath));
	TestFalse(TEXT("损坏JSON应返回错误原因"), Error.IsEmpty());

	const FString MissingFieldJson =
		TEXT("{\"Snapshots\":[{\"MeshPath\":\"/Game/Test/SM_New.SM_New\",\"CenterX\":4,\"CenterY\":5}]}" );
	TestFalse(TEXT("字段不完整的快照应拒绝加载"),
		XPivotSnapshotSerialization::Deserialize(MissingFieldJson, Snapshots, Error));
	TestTrue(TEXT("字段不完整时替换应保持原子性"), Snapshots.Contains(ExistingPath));

	const FString ValidJson =
		TEXT("{\"Version\":\"1.0\",\"Snapshots\":[{\"MeshPath\":\"/Game/Test/SM_New.SM_New\",\"CenterX\":4,\"CenterY\":5,\"CenterZ\":6,\"Timestamp\":\"2026.09.01-00.00.00\"}]}" );
	TestTrue(TEXT("完整快照应成功加载"),
		XPivotSnapshotSerialization::Deserialize(ValidJson, Snapshots, Error));
	TestTrue(TEXT("成功加载后错误信息应清空"), Error.IsEmpty());
	TestEqual(TEXT("成功加载后应整体替换旧快照"), Snapshots.Num(), 1);
	TestFalse(TEXT("成功加载后不应残留旧快照"), Snapshots.Contains(ExistingPath));

	const FX_PivotSnapshot* LoadedSnapshot = Snapshots.Find(FSoftObjectPath(TEXT("/Game/Test/SM_New.SM_New")));
	TestNotNull(TEXT("应加载新快照路径"), LoadedSnapshot);
	if (LoadedSnapshot)
	{
		TestTrue(TEXT("应保留快照中心坐标"), LoadedSnapshot->BoundsCenter.Equals(FVector(4.0, 5.0, 6.0)));
	}
	return true;
}

#endif
