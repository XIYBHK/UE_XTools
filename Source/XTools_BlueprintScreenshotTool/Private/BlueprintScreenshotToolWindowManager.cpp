// Copyright 2024 Gradess Games. All Rights Reserved.


#include "BlueprintScreenshotToolWindowManager.h"
#include "BlueprintScreenshotToolDiffWindowButton.h"
#include "BlueprintScreenshotToolSettings.h"
#include "SBlueprintDiff.h"
#include "Algo/RemoveIf.h"

void UBlueprintScreenshotToolWindowManager::Tick(float DeltaTime)
{
	// UE 最佳实践：避免每帧执行昂贵的UI遍历操作
	// 使用间隔检查机制，仅在需要时检查 Diff 窗口
	TimeSinceLastDiffCheck += DeltaTime;

	if (TimeSinceLastDiffCheck >= DiffCheckInterval)
	{
		AddScreenshotButtonToDiffs();
		TimeSinceLastDiffCheck = 0.f;
	}
}

TStatId UBlueprintScreenshotToolWindowManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBlueprintScreenshotToolWindowManager, STATGROUP_Tickables);
}

bool UBlueprintScreenshotToolWindowManager::IsAllowedToTick() const
{
	return IsValid(this);
}

TSharedPtr<SWidget> UBlueprintScreenshotToolWindowManager::FindParent(TSharedPtr<SWidget> InWidget, const FName& InParentWidgetType)
{
	if (!InWidget.IsValid())
	{
		return nullptr;
	}

	auto Parent = InWidget->GetParentWidget();
	if (Parent.IsValid())
	{
		const auto& Type = Parent->GetType();
		if (Type.IsEqual(InParentWidgetType))
		{
			return Parent;
		}

		return FindParent(Parent, InParentWidgetType);
	}

	return nullptr;
}

TSharedPtr<SWidget> UBlueprintScreenshotToolWindowManager::FindChild(TSharedPtr<SWidget> InWidget, const FName& InChildWidgetType)
{
	if (!InWidget.IsValid())
	{
		return nullptr;
	}

	// UE 最佳实践：使用迭代+队列替代递归，避免栈溢出和提升性能
	// 特别是在复杂 UI 层级中，递归可能导致性能问题
	TQueue<TSharedPtr<SWidget>> WidgetQueue;
	WidgetQueue.Enqueue(InWidget);

	while (!WidgetQueue.IsEmpty())
	{
		TSharedPtr<SWidget> CurrentWidget;
		WidgetQueue.Dequeue(CurrentWidget);

		if (!CurrentWidget.IsValid())
		{
			continue;
		}

		const auto& Type = CurrentWidget->GetType();
		if (Type.IsEqual(InChildWidgetType))
		{
			return CurrentWidget;
		}

		auto* Children = CurrentWidget->GetChildren();
		if (Children)
		{
			for (int32 i = 0; i < Children->Num(); i++)
			{
				WidgetQueue.Enqueue(Children->GetChildAt(i));
			}
		}
	}

	return nullptr;
}

TSet<TSharedPtr<SWidget>> UBlueprintScreenshotToolWindowManager::FindChildren(TSharedPtr<SWidget> InWidget, const FName& InChildWidgetType)
{
	if (!InWidget.IsValid())
	{
		return {};
	}

	// UE 最佳实践：使用迭代+队列替代递归
	TSet<TSharedPtr<SWidget>> Result;
	TQueue<TSharedPtr<SWidget>> WidgetQueue;
	WidgetQueue.Enqueue(InWidget);

	while (!WidgetQueue.IsEmpty())
	{
		TSharedPtr<SWidget> CurrentWidget;
		WidgetQueue.Dequeue(CurrentWidget);

		if (!CurrentWidget.IsValid())
		{
			continue;
		}

		const auto& Type = CurrentWidget->GetType();
		if (Type.IsEqual(InChildWidgetType))
		{
			Result.Add(CurrentWidget);
		}

		auto* Children = CurrentWidget->GetChildren();
		if (Children)
		{
			for (int32 i = 0; i < Children->Num(); i++)
			{
				WidgetQueue.Enqueue(Children->GetChildAt(i));
			}
		}
	}

	return Result;
}

TSet<TSharedPtr<SGraphEditor>> UBlueprintScreenshotToolWindowManager::FindGraphEditors(TSharedRef<SWindow> InWindow)
{
	const auto Widgets = FindChildren(InWindow, TEXT("SGraphEditorImpl"));
	TArray<TSharedPtr<SGraphEditor>> GraphEditors;

	Algo::Transform(Widgets, GraphEditors, [](TSharedPtr<SWidget> Widget) { return StaticCastSharedPtr<SGraphEditor>(Widget); });
	auto Index = Algo::RemoveIf(GraphEditors, [](TSharedPtr<SGraphEditor> GraphEditor) { return !GraphEditor.IsValid(); });

	return TSet<TSharedPtr<SGraphEditor>>(GraphEditors);
}

TSet<TSharedPtr<SGraphEditor>> UBlueprintScreenshotToolWindowManager::FindActiveGraphEditors()
{
	const auto ActiveTab = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!ActiveTab.IsValid())
	{
		return {};
	}

	return FindGraphEditors(ActiveTab.ToSharedRef());
}

TSet<TSharedPtr<SGraphEditor>> UBlueprintScreenshotToolWindowManager::FindAllGraphEditors()
{
	auto Windows = GetWindows();

	TSet<TSharedPtr<SGraphEditor>> GraphEditors;
	for (const auto Window : Windows)
	{
		auto GraphEditorsInWindow = FindGraphEditors(Window);
		GraphEditors.Append(GraphEditorsInWindow);
	}

	return GraphEditors;
}

TArray<TSharedRef<SWindow>> UBlueprintScreenshotToolWindowManager::GetWindows()
{
	TArray<TSharedRef<SWindow>> Windows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
	return Windows;
}

TArray<TSharedRef<SBlueprintDiff>> UBlueprintScreenshotToolWindowManager::GetBlueprintDiffs()
{
	auto Windows = GetWindows();

	TArray<TSharedRef<SBlueprintDiff>> BlueprintDiffs;

	for (const auto Window : Windows)
	{
		const auto Child = FindChild<SBlueprintDiff>(Window);
		if (Child.IsValid())
		{
			BlueprintDiffs.Add(Child.ToSharedRef());
		}
	}

	return BlueprintDiffs;
}

void UBlueprintScreenshotToolWindowManager::AddScreenshotButtonToDiffs()
{
	const auto Diffs = GetBlueprintDiffs();
	for (const auto BlueprintDiff : Diffs)
	{
		AddButtonToDiffWindow(BlueprintDiff);
	}
}

void UBlueprintScreenshotToolWindowManager::AddButtonToDiffWindow(TSharedRef<SBlueprintDiff> InBlueprintDiff)
{
	const auto DiffToolBars = UBlueprintScreenshotToolWindowManager::FindChildren<SMultiBoxWidget>(InBlueprintDiff);
	if (DiffToolBars.Num() <= 0)
	{
		return;
	}

	TArray<TSharedPtr<SMultiBoxWidget>> FilteredToolBars = DiffToolBars.Array();
	FilteredToolBars = FilteredToolBars.FilterByPredicate([](const TSharedPtr<SMultiBoxWidget>& ToolBar)
	{
		bool bTakeScreenshotButtonExists = false;
		auto ToolbarTextToCheck = GetDefault<UBlueprintScreenshotToolSettings>()->DiffToolbarTexts;

		const auto TextBlocks = UBlueprintScreenshotToolWindowManager::FindChildren<STextBlock>(ToolBar);
		for (const auto TextBlock : TextBlocks)
		{
			const auto& ButtonText = TextBlock->GetText();
			const auto NumRemovedElems = ToolbarTextToCheck.RemoveAll([ButtonText](const FText& InText) { return InText.EqualToCaseIgnored(ButtonText); });

			if (NumRemovedElems == 0 && !bTakeScreenshotButtonExists)
			{
				const auto Label = GetDefault<UBlueprintScreenshotToolSettings>()->DiffWindowButtonLabel;
				bTakeScreenshotButtonExists = Label.EqualToCaseIgnored(ButtonText);
			}
		}

		const bool bAppropriateDiffToolBar = ToolbarTextToCheck.Num() <= 0;
		return bAppropriateDiffToolBar && !bTakeScreenshotButtonExists;
	});


	for (auto ToolBar : FilteredToolBars)
	{
		TSharedRef<FBlueprintScreenshotToolDiffWindowButton> NewToolBarButtonBlock(new FBlueprintScreenshotToolDiffWindowButton());
		TSharedRef<FMultiBox> MultiBoxCopy = MakeShareable(new FMultiBox(ToolBar->GetMultiBox().Get()));
		MultiBoxCopy->AddMultiBlock(NewToolBarButtonBlock);
		ToolBar->SetMultiBox(MultiBoxCopy);
		ToolBar->BuildMultiBoxWidget();
	}
}
