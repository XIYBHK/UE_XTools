// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistMisc/BAScopedRollbackTransaction.h"

#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "Misc/ITransaction.h"

void FBAScopedRollbackTransaction::Rollback(const FText& OptionalFailureMsg)
{
	if (!IsOutstanding())
	{
		return;
	}

	if (GUndo)
	{
		GUndo->Apply();
	}

	Cancel();

	if (!OptionalFailureMsg.IsEmpty())
	{
		UE_LOG(LogBlueprintAssist, Warning, TEXT("%s"), *OptionalFailureMsg.ToString());
	}
}
