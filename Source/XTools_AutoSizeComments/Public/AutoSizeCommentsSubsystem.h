// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "AutoSizeCommentsSubsystem.generated.h"

class UEdGraphNode_Comment;

UCLASS()
class XTOOLS_AUTOSIZECOMMENTS_API UAutoSizeCommentsSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	static UAutoSizeCommentsSubsystem* Get();

	/** Called by graph-formatting plugins after changing cached node geometry. */
	UFUNCTION()
	void MarkNodeDirty(UEdGraphNode_Comment* Node);

	bool IsDirty(UEdGraphNode_Comment* Node);

private:
	TSet<TWeakObjectPtr<UEdGraphNode_Comment>> DirtyComments;
};
