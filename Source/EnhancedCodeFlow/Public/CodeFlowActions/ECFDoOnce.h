// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "ECFActionBase.h"
#include "ECFDoOnce.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

UCLASS()
class ENHANCEDCODEFLOW_API UECFDoOnce : public UECFActionBase
{
	GENERATED_BODY()

	friend class UECFSubsystem;

protected:

	TUniqueFunction<void()> ExecFunc;

	bool Setup(TUniqueFunction<void()>&& InExecFunc)
	{
		ExecFunc = MoveTemp(InExecFunc);

		if (ExecFunc)
		{
			return true;
		}
		else
		{
			ensureMsgf(false, TEXT("ECF - do once failed to start. Are you sure the Exec Function is is set properly?"));
			return false;
		}
	}

	void Init() override
	{
		ExecFunc();
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION