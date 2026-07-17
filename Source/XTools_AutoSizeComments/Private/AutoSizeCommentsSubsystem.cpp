// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsSubsystem.h"

#include "EdGraphNode_Comment.h"
#include "Editor.h"

UAutoSizeCommentsSubsystem* UAutoSizeCommentsSubsystem::Get()
{
	return GEditor ? GEditor->GetEditorSubsystem<UAutoSizeCommentsSubsystem>() : nullptr;
}

void UAutoSizeCommentsSubsystem::MarkNodeDirty(UEdGraphNode_Comment* Node)
{
	if (IsValid(Node))
	{
		DirtyComments.Add(Node);
	}
}

bool UAutoSizeCommentsSubsystem::IsDirty(UEdGraphNode_Comment* Node)
{
	return IsValid(Node) && DirtyComments.Remove(Node) > 0;
}
