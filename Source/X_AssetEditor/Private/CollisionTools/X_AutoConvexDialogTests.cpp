/*
 * AutoConvex Slate 生命周期自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CollisionTools/X_AutoConvexDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXAutoConvexDialogWindowLifecycleTest,
	"XTools.AssetEditor.AutoConvexDialog.WindowLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXAutoConvexDialogWindowLifecycleTest::RunTest(const FString& Parameters)
{
	TSharedPtr<SX_AutoConvexDialog> Dialog = SNew(SX_AutoConvexDialog);
	TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("X_AutoConvexDialog", "AutomationTitle", "AutoConvex Automation"))
		.SizingRule(ESizingRule::Autosized)
		[
			Dialog.ToSharedRef()
		];

	FSlateApplication::Get().AddWindow(Window.ToSharedRef());
	Dialog->DialogWindow = Window;
	TWeakPtr<SWindow> WeakWindow = Window;
	TestTrue(TEXT("窗口加入 Slate 后应有效"), WeakWindow.IsValid());

	Window->RequestDestroyWindow();
	Window.Reset();

	// RequestDestroyWindow 将销毁排入 Slate 队列，需要由真实 Slate tick 处理。
	FSlateApplication::Get().Tick();
	FSlateApplication::Get().Tick();

	TestFalse(TEXT("对话框反向窗口引用必须保持弱引用"), Dialog->DialogWindow.IsValid());
	TestTrue(TEXT("确认回调应在窗口有效时可执行"), Dialog->OnConfirm().IsEventHandled());
	TestTrue(TEXT("确认回调应设置确认状态"), Dialog->bConfirmed);
	Dialog.Reset();
	TestFalse(TEXT("Slate 处理销毁队列后窗口弱引用应失效"), WeakWindow.IsValid());

	// 取消路径同样应返回 Handled，并保持取消状态。
	TSharedPtr<SX_AutoConvexDialog> CancelDialog = SNew(SX_AutoConvexDialog);
	TSharedPtr<SWindow> CancelWindow = SNew(SWindow)[CancelDialog.ToSharedRef()];
	FSlateApplication::Get().AddWindow(CancelWindow.ToSharedRef());
	CancelDialog->DialogWindow = CancelWindow;
	TestTrue(TEXT("取消回调应被 Slate 消费"), CancelDialog->OnCancel().IsEventHandled());
	TestFalse(TEXT("取消回调应保持未确认状态"), CancelDialog->bConfirmed);
	CancelWindow.Reset();
	CancelDialog.Reset();
	FSlateApplication::Get().Tick();
	return true;
}

#endif
